/* Author: Damian Amerman-Smith
 * Northbridge MCU's Firmware. 
 *
 * If having "invalid header" issues, unplug board power for ~3 seconds, then hit reset. 
 * Make sure config.h has the right PCB defined
 *
 * TODO (top-to-bottom):
    * Allow CPU request orders/addressing to work
    * Attach Flash memory driver to SPI transactions so CPU can read data in
    * Implement PSRAM
 */

// Libraries
#include <Arduino.h>
#include <string.h>
#include <driver/spi_slave.h>  // Doesn't play nicely with Quad SPI

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

#include "config.h"
#include "constants.h"


// Archaic: NB-Audio hotwiring for System Verification 1... Consider deleting if music still works well
// #include <ESP_I2S.h>        // Possibly unneeded (if NB-SB uart streaming goes well)
// #define I2S_LRC  34
// #define I2S_BCLK 36
// #define I2S_DIN  39
// i2s_data_bit_width_t bps = I2S_DATA_BIT_WIDTH_16BIT;
// i2s_mode_t mode = I2S_MODE_STD;
// i2s_slot_mode_t slot = I2S_SLOT_MODE_STEREO;
// I2SClass i2s;
// #define MSG_SIZE 512
// uint8_t TX_buf[BUF_SIZE + 1];  // Tx buffer
// uint8_t RX_buf[BUF_SIZE + 1];  // Rx buffer
// uint8_t UART_buf[BUF_SIZE];

SoftwareSerial SBuart(UART_RX, UART_TX);
const uint32_t BUF_SIZE = 64;  // Bytes in tx/rx buffers

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
SPIClass hspi(HSPI);
SPIFlash flash(HSPI_CS_FL);
SPIFlash psram(HSPI_CS_PS);  // Not yet working

uint64_t FL_MAX;
uint64_t PS_MAX;
uint32_t ret;
bool fencepost = true;
spi_slave_transaction_t message;  // Transaction struct


// ARCHAIC (replaced by SoftwareSerial) Initializes UART using given pins
// bool init_uart(const uart_port_t port, const uint32_t tx, const uint32_t rx) {
//   strcpy((char*) UART_buf, "This is a UART message from the NB to the SB. ");
//   uart_config = {
//     .baud_rate = UART_BAUD,
//     .data_bits = UART_DATA_8_BITS,
//     .parity = UART_PARITY_DISABLE,
//     .stop_bits = UART_STOP_BITS_1,
//     .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
//   };

//   if (ESP_OK != uart_driver_install(port, BUF_SIZE, BUF_SIZE, 10, &uart_queue, 0)) {
//     Serial.println("Error: Couldn't add UART");
//     return false;
//   }
//   if (ESP_OK != uart_param_config(port, &uart_config)) {
//     Serial.println("Error: Couldn't add UART");
//     return false;
//   }
//   if (ESP_OK != uart_set_pin(port, tx, rx, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE)) {
//     Serial.println("Error: Couldn't add UART");
//     return false;
//   }
//   return true;
// }


// Interrupt and interrupt flag cmd_rdy indicate CPU has a command to send using CMD_RDY/VSPI_D2
bool cmd_rdy = false;
void IRAM_ATTR cmd_isr() {
  cmd_rdy = true;
}


// Initializes single SPI communication. Note: although Quad SPI is desired, the ESP-IDF 'Slave' Driver
// only supports fullduplex (single) transactions and the Halfduplex driver doesn't support ESP32-wroom-32E
bool init_spi(spi_host_device_t host, int cs, bool debug) {
  memset(&spi_bus, 0, sizeof(spi_bus));
  spi_bus.mosi_io_num = VSPI_MOSI;
  spi_bus.miso_io_num = VSPI_MISO;
  spi_bus.quadwp_io_num = -1;
  spi_bus.quadhd_io_num = -1;

  pinMode(VSPI_D2, INPUT);   // CMD_RDY
  pinMode(VSPI_D3, OUTPUT);  // D_RDY

  cmd_rdy = false;
  digitalWrite(VSPI_D3, HIGH);  // High means inactive, falling indicates ready
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
    Serial.println("Error: Couldn't initialize NB as a SPI peripheral");
    return false;
  }
  if (debug) Serial.println("SPI peripheral device initialized");
  return true;
}


// Reads data from given SD card file to Serial Monitor. Note: prefix filenames with '/'
void read_SD(const char filepath[]) {
  if (SD.exists(filepath)) {
    Serial.printf("%s:\n", filepath);
    File f = SD.open(filepath);
    while (f.available()) {
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

// Reads data from given SD card file to SB using UART. Note: buffer is passed by reference,
// and must be instantiated with the required file's size before calling this function
// Note: UART bottlenecked old setup from being able to use UART for continuous music transmission
size_t read_SD_to_SB(const char filepath[], size_t size, uint8_t* buf) {
  if (SD.exists(filepath)) {
    File f = SD.open(filepath);
    size_t sent = 0;
    while (f.available()) {
      for (size_t i = 0; i < TRAN_SIZE; i++) {
        if (f.available()) {
          buf[i] = f.read();
          // SBuart.print((char)buf[i]);
          // Serial.printf("%x", buf[i]);
          sent++;
        } else {
          buf[i] = 0;
        }
      }
      SBuart.write(buf, TRAN_SIZE);  // Untested but want to sent a whole transaction at once

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

// Reads an ~~mp3~~ (currently only WAV) file to a byte steam, then sends it to the Southbridge via UART.
// Buffer will be internally created and is deleted before the end of the function
// Will return status: 0 is failure, otherwise number of bytes sent
size_t send_MP3(const char filepath[]) {
  Serial.println("sodjfoj");
  if (SD.exists(filepath)) {
    size_t size = get_SD_size(filepath);  // Opens file
    size_t sent = 0;
    Serial.printf("File %s is: ", filepath);
    Serial.print(size);
    Serial.print(" bytes. Need ");
    Serial.print(size / TRAN_SIZE);
    Serial.println(" transmissions");

    while (sent < size) {   // Note: this may not need to be a loop
      uint8_t* buf = NULL;  // Allocates buffer
      buf = (uint8_t*)heap_caps_malloc(TRAN_SIZE + 1, MALLOC_CAP_8BIT);
      if (NULL == buf) {
        Serial.println("failed to make buffer");
        return 0;
      }
      memset(buf, '\0', TRAN_SIZE + 1);
      sent += read_SD_to_SB(filepath, size, (uint8_t*)buf);  // Reads file to buffer
      Serial.println("");
      heap_caps_free((void*)buf);
    }

    Serial.println("sent mp3");

    return size;
  } else {
    Serial.printf("Error: %s not found\n", filepath);
    return 0;
  }
}

// Timeout is number of seconds to wait (it'll try every 2.5 sec)
bool init_SD(double timeout) {
  hspi.begin(HSPI_CLK, HSPI_MISO, HSPI_MOSI, HSPI_CS_SD);
  double elapsed = 0;
  while (!SD.begin(HSPI_CS_SD, hspi) & (elapsed < timeout)) {  // Tries to connect for 7.5 seconds
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

void FL_printHex(uint32_t start, uint32_t end) {
  uint8_t test[end - start];
  Serial.printf("Reading from flash:\n");
  flash.readByteArray(start, (uint8_t*)&test, end, true);
  for (uint32_t i = start; i < end; i++) {
    if ((i % (0xFF + 1)) == 0) {
      Serial.printf("\n0x%x\t", i);
    }
    Serial.printf("%x", (char*)test[i]);
  }
  Serial.printf("\nEnd read\n");
}

void FL_printChar(uint32_t start, uint32_t end) {
  uint8_t test[end - start] = { 0 };
  Serial.printf("Reading from flash:\n");
  ret = 0;
  ret = flash.readByteArray(start, (uint8_t*)&test, (end - start), true);
  if (!ret) {
    Serial.println("Error: Couldn't read flash");
    return;
  }
  for (uint32_t i = start; i < end; i++) {
    if ((i % (0xFF + 1)) == 0) {
      Serial.printf("\n0x%x\t", i);
    }
    Serial.printf("%c", (char*)test[i]);
  }
  Serial.printf("\nEnd read\n");
}

// Struct for interpreting NB-CPU commands
struct cmd_data {
  uint8_t dest;
  uint8_t size;
  bool write;
};

// Reads/interprets command from cpu, returning cmd_data struct
struct cmd_data parse_cmd(uint8_t cmd, bool debug) {
  struct cmd_data ret;
  // Gets destination (0 is PSRAM, 1 is Flash, 2 is SD Card, 3 is Southbridge)
  uint8_t dest = (cmd & 0b11000000);
  dest = dest >> 6;
  // Gets request size (0 is whole word, 1 is first half, 2 is second half, 3-6 are bytes 1-4)
  uint8_t size = (cmd & 0b00111100);
  size = size >> 2;
  // Gets if it's a read or write request ()
  bool write = (cmd & 0b00000011);
  ret.dest = dest;
  ret.size = size;
  ret.write = write;

  if (debug) {
    // Serial.printf("dest: %d\tsize: %d\twrite:%d"\n", ret);
    Serial.printf("dest: %d\tsize: %d\twrite:%d\n", dest, size, write);
  }
  return ret;
}

// Queues an SPI input transaction, the NB waiting on a command from the CPU
void create_input_queue(uint8_t* buf, int size) {
  memset(&message, 0, sizeof(message));
  message.flags = 0;      // SPI_TRANS_MODE_DIO
  message.length = size;  // 8-bit cmd, 4-bit buffer, 28-bit addr
  message.tx_buffer = (void*)NULL;
  message.rx_buffer = (void*)buf;
  message.user = (void*)1;

  // Queues message
  if (ESP_OK != spi_slave_queue_trans(cpu_host, &message, portMAX_DELAY)) {
    Serial.println("Error: couldn't request command");
  }
}

// Queues an SPI output transaction, the CPU getting sent data or write results
void create_output_queue(uint8_t* buf, int size) {
  memset(&message, 0, sizeof(message));
  message.flags = 0;      // SPI_TRANS_MODE_DIO
  message.length = size;  // 8-bit cmd, 4-bit buffer, 28-bit addr
  message.tx_buffer = (void*)buf;
  message.rx_buffer = (void*)NULL;
  message.user = (void*)1;

  // Queues message
  if (ESP_OK != spi_slave_queue_trans(cpu_host, &message, portMAX_DELAY)) {
    Serial.println("Error: couldn't request command");
  }
}

// Gets NB-CPU transaction results
void wait_for_queue_results() {
  spi_slave_transaction_t* msg_rcv;  // Transaction struct
  if (ESP_OK != spi_slave_get_trans_result(cpu_host, (spi_slave_transaction_t**)&msg_rcv, portMAX_DELAY)) {
    Serial.println("Error: couldn't receive command");
  }
}

// Signals D_RDY so CPU knows we're ready to transmit
void send_ready() {
  digitalWrite(VSPI_D3, LOW);
  digitalWrite(VSPI_D3, HIGH);
}

void print_cmd_data(struct cmd_data data) {
  Serial.printf("dest: ");
  Serial.print(data.dest, BIN);
  Serial.printf("\tsize: ");
  Serial.print(data.size, BIN);
  Serial.printf("\twrite: ");
  Serial.print(data.write, BIN);
  // Serial.printf("\tReturning: %x" );
  Serial.println();
}

// Clears 64-bit buffer (replacing with 0`)
void clear_buf(uint8_t* buf) {
  for (int i = 0; i < 8; i++) {
    buf[i] = 0;
  }
}

// Prints 64-bit buffer in hexadecimal
void print_buf(uint8_t* buf) {
  for (int i = 0; i < 8; i++) {
    Serial.printf("%02X", buf[i]);
  }
  Serial.printf("\n");
}

uint8_t cmd_rec[8] = { 0 };
bool bring_in_the_olives = false;



// Setup initialization
void setup() {
  Serial.begin(115200);
  randomSeed(time(NULL));
  delay(1);
  SBuart.begin(UART_BAUD);

  Serial.println("HSPI Init...");


  // Flash initialization
  SPI.begin(HSPI_CLK, HSPI_MISO, HSPI_MOSI, HSPI_CS_FL);
  ret = flash.begin(MB(16));
  Serial.printf("\nFlash Init: %d\t\tJEDEC ID: 0x%x\n", ret, flash.getJEDECID());
  FL_MAX = flash.getCapacity();
  flash.setClock(FL_FREQ);
  Serial.printf("%d kB Capacity\n", FL_MAX / 1000);


  // SD initialization
  double timeout = 10;  // Time to wait on SD card
  Serial.printf("\nSD Init: %d\n", init_SD(timeout));

  init_spi(cpu_host, VSPI_CS, true);
  Serial.println("\nNorthbridge initialized");
}


// memset(TX_buf, '\0', BUF_SIZE);  // Prepares communication buffers
// memset(RX_buf, '\0', BUF_SIZE);

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


// // Flash read/write test
// char str2[] = "This is a sample string";


// FL_readChar(10, 10+strlen(str2));
// FL_clear();
// // delay(5000);

// ret = 0;
// int i = 0;
// ret = flash.writeCharArray(10, str2, 10+strlen(str2), true);

// Serial.printf("Sample Text Write: %d\n", ret);
// FL_readChar(10, 10+strlen(str2));
// delay(100);


// Main control loop
void loop() {
  if (fencepost) {
    fencepost = false;
    Serial.println("fencepostin");
    create_input_queue(cmd_rec, 32);
  }
  // Needs one dedicated core to service CPU SPI requests
  if (cmd_rdy) {
    wait_for_queue_results();  // Makes sure cmd is received

    Serial.print("cmd line: ");
    Serial.println(cmd_rec[0], BIN);

    struct cmd_data cmd = parse_cmd(cmd_rec[0], true);

    print_cmd_data(cmd);
    clear_buf(cmd_rec);
    uint8_t rec_data[8] = { 0 };

    if (cmd.write) {
      create_input_queue(rec_data, 64);
    } else {
      create_input_queue(rec_data, 32);
    }
    send_ready();

    Serial.println("waiting for second command");

    wait_for_queue_results();

    Serial.print("addr: ");
    print_buf(rec_data);

    if (!cmd.write) {
      Serial.println("Read command");
      clear_buf(rec_data);
      rec_data[0] = 0xAB;
      rec_data[1] = 0xAB;
      rec_data[2] = 0xDC;
      rec_data[3] = 0xDC;
      create_output_queue(rec_data, 32);
      send_ready();
      wait_for_queue_results();
    } else {
      Serial.println("Write command");

      // Add control flow so only SB-targetted writes trigger send_
      bring_in_the_olives = true;
    }
    clear_buf(cmd_rec);
    create_input_queue(cmd_rec, 32);
    cmd_rdy = false;
    Serial.println("here we go again");
  }

  // If RX'ed a CPU write command to SB, play music
  if (bring_in_the_olives) {
    bring_in_the_olives = false;
    send_MP3("/meglo.wav");
  }
  delay(1);
}
