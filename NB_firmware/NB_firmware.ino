/* Author: Damian Amerman-Smith
 * Northbridge MCU's Firmware. 
 *
 * If having "invalid header" issues, unplug board power for ~3 seconds, then hit reset. 
 *
 *
 * TODO (top-to-bottom):
 * Use interrupts to handle incoming NB and SB requests
   * Add command/address bits to communication to sort memory requests
    * Need to add prefix scraper to extract cmd and addr bits from msgs
 * Post-Initial Checkoff:
    * Implement PSRAM, Flash drivers and API handlers for a given memory transaction
    * Interrupts to handle CPU requests (over SPI)
    * Allow CPU request orders/addressing to work
 */

// Libraries
#include <Arduino.h>
#include <string.h>
// #include <driver/spi_master.h>
#include <driver/spi_slave.h>
#include <driver/uart.h>
#include <esp_random.h>
#include <sdmmc_cmd.h>
#include <esp_vfs_fat.h>
#include <esp_heap_caps.h>
#include <SoftwareSerial.h>
#include <SPIMemory.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>

#include "constants.h"


// Pins for Northbridge-CPU SPI
#define VSPI_CS 18     // big board: 18
#define VSPI_CLK 5     // big board: 5
#define VSPI_MOSI 17   // (D0) big board: 17
#define VSPI_MISO 19   // (D1) big board: 19
#define VSPI_D2 21     // big board: 21
#define VSPI_D3 16     // big board: 16

// Pins for Northbridge-Southbridge UART
#define UART_TX 33    // old NB/MB: 16, big board: 33
#define UART_RX 32    // old NB/MB: 17, big board: 32
#define UART_PORT UART_NUM_2
#define UART_BAUD 57600      // Baud Rate (symbols/sec) for NB-SB UART
SoftwareSerial SBuart(UART_RX, UART_TX);
#define TRAN_SIZE 32768.0

// Pins for Memory Block Connections (main board means Damian's NB/MB)
#define HSPI_CLK 14
#define HSPI_MOSI 13
#define HSPI_MISO 12
#define HSPI_CS_SD 26  // old NB/MB: 4, big board: 26
#define HSPI_CS_FL 15   // old NB/MB: 15, big board: 27
#define HSPI_CS_PS 2   // old NB/MB: 2, big board: 15
#define FL_FREQ 40000000 // in Hz
#define PS_FREQ FL_FREQ

// Archaic: NB-Audio hotwiring for System Verification 1
#include <ESP_I2S.h>        // Possibly unneeded (if NB-SB uart streaming goes well)
#define I2S_LRC  34
#define I2S_BCLK 36
#define I2S_DIN  39
i2s_data_bit_width_t bps = I2S_DATA_BIT_WIDTH_16BIT;
i2s_mode_t mode = I2S_MODE_STD;
i2s_slot_mode_t slot = I2S_SLOT_MODE_STEREO;
I2SClass i2s;


#define MSG_SIZE 512
const uint32_t BUF_SIZE = 512;  // Bytes in tx/rx buffers
uint8_t TX_buf[BUF_SIZE];       // Tx buffer
uint8_t RX_buf[BUF_SIZE];       // Rx buffer
uint8_t UART_buf[BUF_SIZE];

// For VSPI Configuration
spi_host_device_t cpu_host = SPI2_HOST;
spi_bus_config_t cpu_bus;
spi_slave_interface_config_t peripheral_config;
spi_dma_chan_t dma_config = SPI_DMA_DISABLED;
int msg_idx;

// For UART Configuration
uart_config_t uart_config;
QueueHandle_t uart_queue;

// For HSPI Configuration
spi_host_device_t sd_host = SPI1_HOST;
spi_bus_config_t sd_bus;
sdmmc_host_t sd_cfg = SDSPI_HOST_DEFAULT();
sdspi_device_config_t sd;
sdspi_dev_handle_t sd_handle;
spi_bus_config_t bus_cfg;


SPIClass* fl = NULL;
SPIClass* ps = NULL;
SPIFlash flash(HSPI_CS_FL);
uint64_t FL_MAX;
SPIFlash psram(HSPI_CS_PS);
uint64_t PS_MAX;
uint32_t ret;


// Initializes UART using given pins
bool init_uart(const uart_port_t port, const uint32_t tx, const uint32_t rx) {
  strcpy((char*) UART_buf, "This is a UART message from the NB to the SB. ");
  uart_config = {
    .baud_rate = UART_BAUD,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
  };
  
  if (ESP_OK != uart_driver_install(port, BUF_SIZE, BUF_SIZE, 10, &uart_queue, 0)) {
    Serial.println("Error: Couldn't add UART");
    return false;
  }
  if (ESP_OK != uart_param_config(port, &uart_config)) {
    Serial.println("Error: Couldn't add UART");
    return false;
  }
  if (ESP_OK != uart_set_pin(port, tx, rx, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE)) {
    Serial.println("Error: Couldn't add UART");
    return false;
  }
  return true;
}


// Initializes quad SPI communication.
bool init_qspi(spi_host_device_t host, spi_bus_config_t spi_bus, int d0, int d1, int d2, int d3, int clk, int cs) {
  spi_bus.data0_io_num = d0;
  spi_bus.data1_io_num = d1;
  spi_bus.data2_io_num = d2;
  spi_bus.data3_io_num = d3;
  spi_bus.sclk_io_num = clk;
  spi_bus.quadwp_io_num = -1;
  spi_bus.quadhd_io_num = -1;
  
  peripheral_config = {
    .spics_io_num = cs,
    .flags = 0,
    .queue_size = 1024,
    .mode = 0,
    .post_setup_cb = 0,
    .post_trans_cb = 0,
  };
  
  if (ESP_OK != spi_bus_initialize(host, &spi_bus, SPI_DMA_DISABLED)) {
    Serial.println("Error: Couldn't initialize NB as a QSPI peripheral");
    return false;
  }

  if (ESP_OK != spi_slave_initialize(host, &spi_bus, &peripheral_config, SPI_DMA_DISABLED)) {
    Serial.println("Error: Couldn't initialize NB as a QSPI peripheral");
    return false;
  } 
  return true;
}


// Initializes single SPI communication
bool init_spi(spi_host_device_t host, spi_bus_config_t spi_bus, int mosi, int miso, int clk, int cs) {
  spi_bus.mosi_io_num = mosi;
  spi_bus.miso_io_num = miso;
  spi_bus.sclk_io_num = clk;
  spi_bus.quadwp_io_num = -1;
  spi_bus.quadhd_io_num = -1;

  if (ESP_OK != spi_bus_initialize(host, &spi_bus, SPI_DMA_DISABLED)) {
    Serial.println("Error: Couldn't initialize SPI bus in peripheral mode");
    return false;
  } else {
    return true;
  }
}


// // Queues a variable length SPI message to CPU. payload is message to send, length is bytes to send
// Needs to be adapted to actually be usable...
void transmit_SPI(uint8_t* payload, uint32_t length) { 
  //   Serial.println("SPI sending message");
  //   if (length/8 < MSG_SIZE) {
  //     Serial.println("SPI sending small message");

  //     memset(TX_buf, '\0', BUF_SIZE);
  //     memset(RX_buf, '\0', BUF_SIZE);
  //     strcpy((char*) TX_buf, (char*) payload);
  //     spi_slave_transaction_t message;        // Transaction struct    
  //     memset(&message, 0, sizeof(message));
  //     message = {
  //       .flags = 0,
  //       .length = 2*length,
  //       .trans_len = length,
  //       .tx_buffer = (void*) &TX_buf,
  //       .rx_buffer = (void*) &RX_buf,
  //       .user = (void*) 0,
  //     };
  //     spi_slave_queue_trans(cpu_host, &message, portMAX_DELAY);
  //     // Holds until communication happens
  //     // if(ESP_OK != spi_slave_get_trans_result(host, (spi_slave_transaction_t**)&message, portMAX_DELAY)) {
  //     //   Serial.println("Error: couldn't receive message");
  //     //   return;
  //     // }
  //     // Receiving buffer has info
  //     Serial.print("SPI CPU: ");
  //     Serial.println((uint32_t)RX_buf, HEX);
  //     Serial.println((uint32_t)RX_buf, BIN);

  //   } else {                  // Need to send over multiple messages
  //     Serial.println("SPI sending large message:");
      
  //     int payload_idx = 0;
  //     int sent = 0;

  //     // Serial.println("before while loop");
  //     while(sent < length) {
  //       // Serial.println("In while loop");

  //       char pt[MSG_SIZE] = "";
  //       int msg_lngth = MSG_SIZE;
  //       if (sent + MSG_SIZE < length) {
  //         strncpy(pt, (char*) payload + sent, MSG_SIZE);
  //         sent += MSG_SIZE;
  //         strncpy((char*) TX_buf, (char*) pt, MSG_SIZE);
  //       } else {
  //         strncpy(pt, (char*) payload + sent, length - sent);
  //         msg_lngth = length - sent;
  //         sent = length;
  //       }
  //       memset(TX_buf, '\0', BUF_SIZE);
  //       memset(RX_buf, '\0', BUF_SIZE);
  //       strncpy((char*) TX_buf, (char*) pt, msg_lngth);

  //       Serial.write((char*) TX_buf, msg_lngth);
  //       Serial.println("");
  //       // Serial.write((char*) pt2, MSG_SIZE);
  //       // Serial.println("");
  //       spi_slave_transaction_t message;        // Transaction struct    
  //       memset(&message, 0, sizeof(message));
  //       message = {
  //         .flags = 0,
  //         .length = (size_t) msg_lngth/8,
  //         .trans_len = (size_t) msg_lngth/8,
  //         .tx_buffer = (void*) &TX_buf,
  //         .rx_buffer = (void*) NULL,
  //         .user = (void*) 0,
  //       };
  //       spi_slave_queue_trans(cpu_host, &message, portMAX_DELAY);
        
        
  //       Serial.println("queued");


  //       // Holds until communication happens
  //       if(ESP_OK != spi_slave_get_trans_result(cpu_host, (spi_slave_transaction_t**)&message, portMAX_DELAY)) {
  //         Serial.println("Error: couldn't receive message");
  //         return;
  //       }
  //       // // Receiving buffer has info
  //       Serial.print("SPI CPU: ");
  //       Serial.write(RX_buf, HEX);
  //       Serial.println("");
  //     }
  //     // Serial.println("after while loop");
  //   }
}


// Reads data from given SD card file to Serial Monitor. Note: prefix filenames with '/'
void read_SD(const char filepath[]) {
  if (SD.exists(filepath)) {
    Serial.printf("%s:\n", filepath);
    File f = SD.open(filepath);
    while(f.available()) {
      Serial.write(f.read());
    }
    f.close();
    Serial.println("\n");
  } else {
    Serial.printf("Error: %s doesn't exist\n", filepath);
  }
}


// Gets SD card size
size_t get_SD_size(const char filepath[]) {
  if (SD.exists(filepath)) {
    File f = SD.open(filepath);
    size_t size = f.size();
    return size;
  } else {
    Serial.printf("Error: %s doesn't exist\n", filepath);
    return 0;
  }
}


// Reads data from given SD card file to data buffer. Note: buffer is passed by reference,
// and must be instantiated with the required file's size before calling this function
// Note: UART bottlenecked old setup from being able to use UART for continuous music transmission
size_t read_SD2buf(const char filepath[], size_t size, uint8_t* buf) {
  if (SD.exists(filepath)) {
    File f = SD.open(filepath);

    size_t sent = 0;
    while(f.available()) {
      for(size_t i = 0; i < TRAN_SIZE; i++) {
        if (f.available()) {
          buf[i] = f.read();
          // i2s.write(buf[i]);
          SBuart.print((char)buf[i]);
          Serial.printf("%x", buf[i]);

          sent++;
        } else {
          buf[i] = 0;
        }
      }
      //Sends buffer over UART...
      // Serial.println((char*)   buf);
      // for (int i = 0; i < TRAN_SIZE; i++) {
      //   Serial.print((char)buf[i]); // for debugging
      //   Serial.print(",");
      // }
      // Serial.println("");
    }
    f.close();
    Serial.println("exiting read_SD2buf");
    return sent;
  } else {
    Serial.printf("Error: %s doesn't exist\n", filepath);
    return 0;
  }
}


// Reads an ~~mp3~~ (current WAV) file to a byte steam, then sends it to the Southbridge via UART.
// Buffer will be internally created and is deleted before the end of the function
// Will return status: 0 is failure, otherwise number of bytes sent.
// Timeout is delay after failure, in seconds
size_t send_MP3(const char filepath[], double timeout) {
  if (SD.exists(filepath)) {
    size_t size = get_SD_size(filepath);                      // Opens file
    size_t sent = 0;
    Serial.printf("File %s is: ", filepath);
    Serial.print(size);
    Serial.print(" bytes. Need ");
    Serial.print(size / TRAN_SIZE);
    Serial.println(" transmissions");

    while (sent < size) { 
      uint8_t* buf = NULL;                              // Allocates buffer
      buf = (uint8_t*)heap_caps_malloc(TRAN_SIZE+1, MALLOC_CAP_8BIT);
      if (NULL == buf) {
        Serial.println("failed to make buffer");
        return 0;
      }
      memset(buf, '\0', TRAN_SIZE+1);

      // Serial.println("made buffer");
      sent += read_SD2buf(filepath, size, (uint8_t*) buf); // Reads file to buffer
      Serial.println("");
      heap_caps_free((void*) buf);
    }
    
    Serial.println("sent mp3");

    return size;
  } else {
    Serial.printf("Error: %s not found\n", filepath);
    delay(timeout*1000);
    return 0;
  }

}


// Timeout is number of seconds it'll try (every 2.5 sec)
bool init_SD(double timeout) {
  SPI.begin(HSPI_CLK, HSPI_MISO, HSPI_MOSI, HSPI_CS_SD);
  double elapsed = 0;
  while (!SD.begin(HSPI_CS_SD) & (elapsed < timeout)) {                         // Tries to connect for 7.5 seconds
    Serial.println("Error: SD Card mount failed");
    delay(2500);
    elapsed += 2.5;
  }
  uint8_t sd_type = SD.cardType();
  if (sd_type == CARD_NONE) {
    Serial.println("Error: no SD Card attached");
    return false;
  }
  Serial.printf("\nSD Card size: %d\nSectors: %d\nTotal: %d Bytes\nUsed: %d Bytes\n\n", SD.cardSize(), SD.numSectors(), SD.sectorSize(), SD.totalBytes(), SD.usedBytes());
  return true;
}


// Wipes entire flash chip. Be cautious about using this function
bool FL_clear() {
  if (!flash.eraseChip()) {
    Serial.println("Error: Couldn't clear Flash Memory");
    return false; 
  }
  return true;
}


void FL_readHex(uint32_t start, uint32_t end) {
  uint8_t test[end - start];
  Serial.printf("Reading from flash:\n");
  flash.readByteArray(start, (uint8_t*) &test, end, true);
  for (uint32_t i = start; i < end; i++) {
    if ((i % (0xFF+1)) == 0) {
      Serial.printf("\n0x%x\t", i);
    }
    Serial.printf("%x", (char*) test[i]);
  }
  Serial.printf("\nEnd read\n");
}


void FL_readChar(uint32_t start, uint32_t end) {
  uint8_t test[end - start];
  Serial.printf("Reading from flash:\n");
  flash.readByteArray(start, (uint8_t*) &test, end, true);
  for (uint32_t i = start; i < end; i++) {
    if ((i % (0xFF+1)) == 0) {
      Serial.printf("\n0x%x\t", i);
    }
    Serial.printf("%c", (char*) test[i]);
  }
  Serial.printf("\nEnd read\n");
}


void setup() {
  Serial.begin(115200);
  randomSeed(time(NULL));
  delay(1);

  strcpy((char*) TX_buf, "This is a SPI message from the NB to the CPU.");

  memset(TX_buf, '\0', BUF_SIZE);  // Prepares communication buffers
  memset(RX_buf, '\0', BUF_SIZE);
  SBuart.begin(UART_BAUD);

  Serial.println("HSPI Init...");
  
  // Initializing flash memory

  // External SPI is causing an issue with internal flash when resetting
  // TODO: to turn SPI on when needed but off when done
  SPI.begin(HSPI_CLK, HSPI_MISO, HSPI_MOSI, HSPI_CS_FL);
  ret = flash.begin(MB(16));
  Serial.printf("\nFlash Init: %d\n", ret);
  FL_MAX = flash.getCapacity();
  // flash.setClock(FL_FREQ);
  Serial.printf("%d kB Capacity\n", FL_MAX/1000);
  
  // // Initalizing PSRAM (in-progress, not yet working)
  // ps->begin(HSPI_CLK, HSPI_MISO, HSPI_MOSI, HSPI_CS_PS);
  // ret = psram.begin(8000000);                     
  // PS_MAX = psram.getCapacity();
  // Serial.printf("\nPSRAM Init: %d, cap: %d\n", ret, PS_MAX);
  // psram.setClock(PS_FREQ);
  // char str[] = "This is a sample string"; 
  // psram.writeCharArray(0, str, strlen(str));

  // PSRAM allocation test
  void* norm = NULL;
  norm = malloc(10*sizeof(int));
  Serial.printf("norm: %x\n", (int) norm);
  free(norm);

  // // SD initialization
  // double timeout = 0;       // Time to wait on SD card
  // Serial.printf("\nSD Init: %d\n", init_SD(timeout));

  // void* ps = NULL;
  // if (!psramInit()) {
  //   Serial.println("Couldn't init psram");
  //   delay(1000);
  // }
  // ps = ps_malloc(10*sizeof(int));
  // Serial.printf("ps: %x\n", (int) ps);
  // free(ps);

  // delay(2500)
  // heap_caps_print_heap_info(MALLOC_CAP_8BIT);
  // heap_caps_malloc_extmem_enable(2048);
  // heap_caps_print_heap_info(MALLOC_CAP_SPIRAM);

  // Serial.printf("Init QSPI: %d\n", init_qspi(cpu_host, cpu_bus, VSPI_MOSI, VSPI_MISO, VSPI_D2, VSPI_D3, VSPI_CLK, VSPI_CS));

  // Flash read/write test
  char str2[] = "This is a sample string";

  FL_clear();
  delay(1000);
  Serial.printf("Sample Text Write: %d\n", flash.writeCharArray(0, str2, strlen(str2)));
  delay(100);
  FL_readChar(0, strlen(str2));
  SPI.end();

  // delay(100);

  FL_readChar(0, strlen(str2));

  SPI.begin(HSPI_CLK, HSPI_MISO, HSPI_MOSI, HSPI_CS_FL);


  FL_readChar(0, strlen(str2));
  
  Serial.println("\nNorthbridge initialized");
}



void loop() {
  memset(TX_buf, '\0', BUF_SIZE);  // Prepares communication buffers
  memset(RX_buf, '\0', BUF_SIZE);
  strcpy((char*) TX_buf, "This is a SPI message from the NB to the CPU.");
  
  spi_transaction_t message;        // Transaction struct
  memset(&message, 0, sizeof(message));
    
  // Tests NB-SB UART Music streaming
  {
    int idx = random(0, NUM_WAV-1);
    delay(1);
    send_MP3(wavs[idx], 2.5);
  }

  message = {
    .flags = 0,
    .length = MSG_SIZE,
    .trans_len = MSG_SIZE,
    .tx_buffer = (void*) &TX_buf,
    .rx_buffer = (void*) &RX_buf,
    .user = (void*) msg_idx++,
  };

  // Queues message
  spi_slave_queue_trans(host, &message, portMAX_DELAY);

  // Currently, this holds until the SPI transaction completes (polling)
  // May implement multi-core usage/communication interrupts later
  if(ESP_OK != spi_slave_get_trans_result(host, (spi_slave_transaction_t**)&message, portMAX_DELAY)) {
    Serial.println("Error: couldn't receive message");
    return;
  }
  // Prints received data
  Serial.print("SPI CPU: ");
  Serial.println((char*) RX_buf);

  // // Tests UART NB->SB Communication
  // memset(UART_buf, '\0', BUF_SIZE);
  // strcpy((char*) UART_buf, "This is a UART message from the NB to the SB. \n");
  // delay(1000);
  
  // // Tests UART SB->NB Communication
  // memset(UART_buf, '\0', BUF_SIZE);
  // uart_read_bytes(UART_PORT, UART_buf, BUF_SIZE - 1, 1000);  
  // Serial.print("UART SB: ");
  // Serial.println((char*) UART_buf);
  // uart_flush(UART_PORT);

  // delay(1000);
}
