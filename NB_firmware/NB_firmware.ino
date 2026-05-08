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
#include <driver/spi_slave.h>       // Doesn't play nicely with Quad SPI

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
#define VSPI_CLK 18     // big board: 18
#define VSPI_CS 5     // big board: 5
#define VSPI_MOSI 17   // (D0) old NB/MB: 23, big board: 17
#define VSPI_MISO 19   // (D1) big board: 19
#define VSPI_D2 21     // big board: 21
#define VSPI_D3 16     // old NB/MB: 22, big board: 16

// Pins for Northbridge-Southbridge UART
#define UART_TX 33    // old NB/MB: 16, big board: 33
#define UART_RX 32    // old NB/MB: 17, big board: 32
#define UART_PORT UART_NUM_2
#define UART_BAUD 74880      // Baud Rate (symbols/sec) for NB-SB UART
SoftwareSerial SBuart(UART_RX, UART_TX);
#define TRAN_SIZE 32768.0

// Pins for Memory Block Connections (main board means Damian's NB/MB)
#define HSPI_CLK 14
#define HSPI_MOSI 13
#define HSPI_MISO 12
#define HSPI_CS_SD 26  // old NB/MB: 4, big board: 26
#define HSPI_CS_FL 27   // old NB/MB: 15, big board: 27
#define HSPI_CS_PS 15   // old NB/MB: 2, big board: 15
#define FL_FREQ 1000 // in Hz
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
const uint32_t BUF_SIZE = 64;  // Bytes in tx/rx buffers
uint8_t TX_buf[BUF_SIZE+1];       // Tx buffer
uint8_t RX_buf[BUF_SIZE+1];       // Rx buffer
uint8_t UART_buf[BUF_SIZE];

// For VSPI Configuration
spi_host_device_t cpu_host = SPI3_HOST;
spi_bus_config_t spi_bus;
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


SPIClass* fl;
SPIClass* ps;
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


bool cmd_rdy;
void IRAM_ATTR cmd_isr() {
  cmd_rdy = true;
}


// Initializes single SPI communication. Note: although Quad SPI is desired, the ESP-IDF 'Slave' Driver
// only supports fullduplex (single) transactions and the Halfduplex driver doesn't support ESP32-wroom-32E
bool init_spi(spi_host_device_t host, int cs) {
  memset(&spi_bus, 0, sizeof(spi_bus));
  spi_bus.mosi_io_num = VSPI_MOSI;
  spi_bus.miso_io_num = VSPI_MISO;
  spi_bus.quadwp_io_num = -1;
  spi_bus.quadhd_io_num = -1;

  pinMode(VSPI_D2, INPUT);  // CMD_RDY
  pinMode(VSPI_D3, OUTPUT); // D_RDY

  cmd_rdy = false;
  digitalWrite(VSPI_D3, HIGH);    // High means inactive, falling indicates ready
  attachInterrupt(VSPI_D2, cmd_isr, FALLING);



  spi_bus.max_transfer_sz = 4096;
  spi_bus.sclk_io_num = VSPI_CLK;
  spi_bus.flags = 0;
  
  peripheral_config = {
    .spics_io_num = cs,
    .flags = 0,
    .queue_size = 3,
    .mode = 0,                       
    .post_setup_cb = NULL,
    .post_trans_cb = NULL,
  };
  
  if (ESP_OK != spi_slave_initialize(host, &spi_bus, &peripheral_config, 0)) {
    Serial.println("Error: Couldn't initialize NB as a QSPI peripheral");
    return false;
  } 
  Serial.println("SPI peripheral device initialized");
  return true;
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

// // Reads data from given SD card file to data buffer. Note: buffer is passed by reference,
// // and must be instantiated with the required file's size before calling this function
// // Note: UART bottlenecked old setup from being able to use UART for continuous music transmission
// size_t read_SD2buf(const char filepath[], size_t size, uint8_t* buf) {
//   if (SD.exists(filepath)) {
//     File f = SD.open(filepath);

//     size_t sent = 0;
//     while(f.available()) {
//       for(size_t i = 0; i < TRAN_SIZE; i++) {
//         if (f.available()) {
//           buf[i] = f.read();
//           // i2s.write(buf[i]);
//           SBuart.print((char)buf[i]);
//           Serial.printf("%x", buf[i]);

//           sent++;
//         } else {
//           buf[i] = 0;
//         }
//       }
//       //Sends buffer over UART...
//       // Serial.println((char*)   buf);
//       // for (int i = 0; i < TRAN_SIZE; i++) {
//       //   Serial.print((char)buf[i]); // for debugging
//       //   Serial.print(",");
//       // }
//       // Serial.println("");
//     }
//     f.close();
//     Serial.println("exiting read_SD2buf");
//     return sent;
//   } else {
//     Serial.printf("Error: %s doesn't exist\n", filepath);
//     return 0;
//   }
// }
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
          i++;
          buf[i] = f.read();
          f.read();
          f.read();
          // i2s.write(buf[i]);
          SBuart.print((char)buf[i]);
          // Serial.printf("%x", buf[i]);

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



// // Reads an ~~mp3~~ (currently only WAV) file to a byte steam, then sends it to the Southbridge via UART.
// // Buffer will be internally created and is deleted before the end of the function
// // Will return status: 0 is failure, otherwise number of bytes sent.
// // Timeout is delay after failure, in seconds
// size_t send_MP3(const char filepath[], double timeout) {
//     Serial.println("entering send_MP3");
//   if (SD.exists(filepath)) {
//     size_t size = get_SD_size(filepath);                      // Opens file
//     size_t sent = 0;
//     Serial.printf("File %s is: ", filepath);
//     Serial.print(size);
//     Serial.print(" bytes. Need ");
//     Serial.print(size / TRAN_SIZE);
//     Serial.println(" transmissions");

//     while (sent < size) { 
//       uint8_t* buf = NULL;                              // Allocates buffer
//       buf = (uint8_t*)heap_caps_malloc(TRAN_SIZE+1, MALLOC_CAP_8BIT);
//       if (NULL == buf) {
//         Serial.println("failed to make buffer");
//         return 0;
//       }
//       memset(buf, '\0', TRAN_SIZE+1);

//       // Serial.println("made buffer");
//       sent += read_SD2buf(filepath, size, (uint8_t*) buf); // Reads file to buffer
//       Serial.println("");
//       heap_caps_free((void*) buf);
//     }
    
//     Serial.println("sent mp3");

//     return size;
//   } else {
//     Serial.printf("Error: %s not found\n", filepath);
//     delay(timeout*1000);
//     return 0;
//   }

// }

// Reads an mp3 file to a byte steam, then sends it to the Southbridge via UART.
// Buffer will be internally created and is deleted before the end of the function
// Will return status: 0 is failure, otherwise number of bytes sent 
size_t send_MP3(const char filepath[]) {
  Serial.println("sodjfoj");
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
    return 0;
  }

}

SPIClass hspi(HSPI);

// Timeout is number of seconds it'll try (every 2.5 sec)
bool init_SD(double timeout) {
  hspi.begin(HSPI_CLK, HSPI_MISO, HSPI_MOSI, HSPI_CS_SD);
  double elapsed = 0;
  while (!SD.begin(HSPI_CS_SD, hspi) & (elapsed < timeout)) {                         // Tries to connect for 7.5 seconds
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
  uint8_t test[end - start] = {0};
  Serial.printf("Reading from flash:\n");
  ret = 0;
  ret = flash.readByteArray(start, (uint8_t*) &test, (end-start), true);
  if (!ret) {
    Serial.println("Error: Couldn't read flash");
    return;
  }
  for (uint32_t i = start; i < end; i++) {
    if ((i % (0xFF+1)) == 0) {
      Serial.printf("\n0x%x\t", i);
    }
    Serial.printf("%c", (char*) test[i]);
  }
  Serial.printf("\nEnd read\n");
}




TaskHandle_t Task0;
// void Tx_Music() {
//   // while (1) {
//     // Tests NB-SB UART Music streaming
//     Serial.println("Sending song");
//     int idx = random(0, NUM_WAV-1);
//     delay(1);
//     send_MP3(wavs[idx], 2.5);
// // 
//   // }
// }



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
  SPI.begin(HSPI_CLK, HSPI_MISO, HSPI_MOSI, HSPI_CS_FL);
  ret = flash.begin(MB(16));
  Serial.printf("\nFlash Init: %d\t\tJEDEC ID: 0x%x\n", ret, flash.getJEDECID());
  FL_MAX = flash.getCapacity();
  flash.setClock(FL_FREQ);
  Serial.printf("%d kB Capacity\n", FL_MAX/1000);
  
  // // Initalizing PSRAM (in-progress, not yet working)
  // ps->begin(HSPI_CLK, HSPI_MISO, HSPI_MOSI, HSPI_CS_PS);
  // ret = psram.begin(8000000);                     
  // PS_MAX = psram.getCapacity();
  // Serial.printf("\nPSRAM Init: %d, cap: %d\n", ret, PS_MAX);
  // psram.setClock(PS_FREQ);
  // char str[] = "This is a sample string"; 
  // psram.writeCharArray(0, str, strlen(str));

  // // PSRAM allocation test
  // void* norm = NULL;
  // norm = malloc(10*sizeof(int));
  // Serial.printf("norm: %x\n", (int) norm);
  // free(norm);

  // SD initialization
  double timeout = 10;       // Time to wait on SD card
  Serial.printf("\nSD Init: %d\n", init_SD(timeout));

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


  // Flash read/write test
  char str2[] = "This is a sample string";


  // FL_readChar(10, 10+strlen(str2));
  // FL_clear();
  // // delay(5000);

  // ret = 0;
  // int i = 0;
  // ret = flash.writeCharArray(10, str2, 10+strlen(str2), true);

  // Serial.printf("Sample Text Write: %d\n", ret);
  // FL_readChar(10, 10+strlen(str2));
  // delay(100);

  // xTaskCreatePinnedToCore((TaskFunction_t) Tx_Music, "TxMusic", 10000, NULL, 2, &Task0, 0);

  Serial.println("\nNorthbridge initialized");
  Serial.printf("Init QSPI: %d\n", init_spi(cpu_host, VSPI_CS));
}


struct cmd_addr{
  uint8_t cmd;
  uint32_t addr;
};

// QSPI transaction to receive read/write command & address from CPU
struct cmd_addr cpu_recv_cmd(bool debug) {
  struct cmd_addr ca = {0};
  struct cmd_addr err = {0};
  uint32_t buf[2] = {0};
  // static uint8_t rx_cmd_buf[5] __attribute__((aligned(5)));
  // memset(rx_cmd_buf, 0, 4*sizeof(uint8_t));
  spi_slave_transaction_t message;        // Transaction struct
  spi_slave_transaction_t* msg_rcv;
  memset(&message, 0, sizeof(message));
  message.flags = 0; // SPI_TRANS_MODE_DIO
  message.length = 32;  // 8-bit cmd, 4-bit buffer, 28-bit addr
  message.tx_buffer = (void*) NULL;
  message.rx_buffer = (void*) &buf[0];
  message.user = (void*) 1;  

  // Queues message
  if(ESP_OK != spi_slave_queue_trans(cpu_host, &message, portMAX_DELAY)) {
    Serial.println("Error: couldn't request command");
    return err;
  }

  // Currently, this holds until the SPI transaction completes (polling)
  // May implement multi-core usage/communication interrupts later
  if(ESP_OK != spi_slave_get_trans_result(cpu_host, (spi_slave_transaction_t**)&msg_rcv, portMAX_DELAY)) {
    Serial.println("Error: couldn't receive command");
    return err;
  }

  memset(&message, 0, sizeof(message));
  message.flags = 0; // SPI_TRANS_MODE_DIO
  message.length = 32;  // 8-bit cmd, 4-bit buffer, 28-bit addr
  message.tx_buffer = (void*) NULL;
  message.rx_buffer = (void*) &buf[1];
  message.user = (void*) 1;  


  // Queues message
  if(ESP_OK != spi_slave_queue_trans(cpu_host, &message, portMAX_DELAY)) {
    Serial.println("Error: couldn't request command");
    return err;
  }

  // Currently, this holds until the SPI transaction completes (polling)
  // May implement multi-core usage/communication interrupts later
  if(ESP_OK != spi_slave_get_trans_result(cpu_host, (spi_slave_transaction_t**)&msg_rcv, portMAX_DELAY)) {
    Serial.println("Error: couldn't receive command");
    return err;
  }

  if (debug) {
    Serial.printf("cmd: 0x%x\n", buf[0]);
    Serial.printf("addr: 0x%x\n", buf[1]);
  }
  ca.cmd = buf[0];
  ca.addr = buf[1];
  return ca;
}

// Reads/interprets command to 
uint32_t parse_cmd(uint8_t cmd, uint32_t addr, bool debug) {
  // Gets destination (0 is PSRAM, 1 is Flash, 2 is SD Card, 3 is Southbridge)
  uint8_t dest = (cmd & (1<<7)) | (cmd & (1<<6));
  dest = dest>>6;

  // Gets request size (0 is whole word, 1 is first half, 2 is second half, 3-6 are bytes 1-4)
  uint8_t size = (cmd & (1<<5)) | (cmd & (1<<4)) | (cmd & (1<<3)) | (cmd & (1<<2));
  size = size >>2;

  // Gets if it's a read or write request ()
  bool write = (cmd & 1<<1) || (cmd & 1);

  switch (dest) {
    case 0:   // PSRAM
      Serial.println("Oops, we still need to implement PSRAM access");


      break;

    case 1:   // Flash
      if (write) {  // Write to Flash
        // ret = flash.write();      

      } else {      // Read from Flash
        
        // ret = flash.readByteArray(addr, (uint8_t*) &test, (e), true);
        if (!ret) {
          Serial.println("Error: Couldn't read flash");
        return 0xFFFFFFFF;    // Returning error
        }

      }
      break;

    case 2:   // SD Card
      Serial.println("Oops, we still need to implement SD card access from the CPU");
      break;
    case 3:  // Southbridge
      Serial.println("Oops, we still need to implement SB access from the CPU");
      break;
    default:
      Serial.println("Error: Couldn't parse command");
      break;
  }  


  if (debug) {
    Serial.println("CPU asked for:");
    Serial.printf("dest: ");
    Serial.print(dest, BIN);
    Serial.printf("\tsize: ");
    Serial.print(size, BIN);
    Serial.printf("\tdest: ");
    Serial.print(write, BIN);
    // Serial.printf("\tReturning: %x" );
    Serial.println("");
  }


  return 0;
}


void loop() {   
  // Needs one dedicated core to service CPU SPI requests
  if (cmd_rdy) {
    // Serial.println("Getting CMD_RDY trigger");

    // Gets CPU command and address (no actual data transfer yet)
    struct cmd_addr ca = cpu_recv_cmd(false);

    Serial.printf("cmd: 0x%x\taddr: 0x%x\t", ca.cmd, ca.addr);
    // memset(RX_buf, '\0', BUF_SIZE+1);
    // uint32_t buf = 0x12345678;        // Replace with actual read/write

    uint32_t buf = parse_cmd(ca.cmd, ca.addr, true);

    spi_slave_transaction_t message;        // Transaction struct
    spi_slave_transaction_t* msg_rcv;
    memset(&message, 0, sizeof(message));
    message.flags = 0;
    message.length = 32;
    message.tx_buffer = (void*) &buf;

    message.rx_buffer = (void*) 0;
    message.user = (void*) 1;

    Serial.printf("Queuing: 0x%x\n", buf);

    // Queues message
    if(ESP_OK != spi_slave_queue_trans(cpu_host, &message, portMAX_DELAY)) {
      Serial.println("Error: couldn't queue message");
      return;
    }

    digitalWrite(VSPI_D3, LOW);
    delay(10);
    digitalWrite(VSPI_D3, HIGH);

    // Currently, this holds until the SPI transaction completes (polling)
    // If I cannot find a way to do SPI interrupts, move the UART transaction to a different core
    if(ESP_OK != spi_slave_get_trans_result(cpu_host, (spi_slave_transaction_t**)&msg_rcv, portMAX_DELAY)) {
      Serial.println("Error: couldn't send data");
      return;
    }

    
    cmd_rdy = false;
  }
  send_MP3("/meglo.wav");
  Serial.println("\n\ndone");
}
