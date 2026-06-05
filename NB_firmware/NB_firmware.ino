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
#include "memory.h"

const int NUM_WAV = 6;
char* wavs[] = {
  (char*)"/AIC_MitB.wav",
  (char*)"/KV_HIG.wav",
  (char*)"/meglo.wav",
  (char*)"/PB_ShSe.wav",
  (char*)"/RA_NGGYU.wav",
  (char*)"/8_bit.wav"
};


SoftwareSerial SBuart(UART_RX, UART_TX);
const uint32_t BUF_SIZE = 64;  // Bytes in tx/rx buffers

// For VSPI (NB-SB) Configuration
spi_host_device_t cpu_host = SPI3_HOST;
spi_bus_config_t spi_bus;
spi_slave_interface_config_t peripheral_config;
spi_dma_chan_t dma_config = SPI_DMA_DISABLED;
int msg_idx;

// Variables for CPU-NB SPI communication
uint64_t cmd_rec = { 0 };
uint64_t rec_data = { 0 };
uint32_t payload = { 0 };
uint32_t addr = { 0 };
#define  SRL_MAX 1024
char     srl_buf[1025] = { '\0' };
uint32_t srl_idx = 0;

// bool bring_in_the_olives = false;   

// For UART Configuration
uart_config_t uart_config;
QueueHandle_t uart_queue;

// For HSPI Configuration
spi_host_device_t     sd_host = SPI1_HOST;
spi_bus_config_t      sd_bus;
sdmmc_host_t          sd_cfg = SDSPI_HOST_DEFAULT();
sdspi_device_config_t sd;
sdspi_dev_handle_t    sd_handle;
spi_bus_config_t      bus_cfg;
SPIClass hspi(HSPI);
SPIFlash flash(HSPI_CS_FL);
SPIFlash psram(HSPI_CS_PS);  // Not yet working

uint64_t FL_MAX;
uint64_t PS_MAX;
uint32_t ret;
bool fencepost = true;
spi_slave_transaction_t message;  // Transaction struct

// Flash Indexing and buffering for writes
bool secInit = false;           // Says if the secBuf been initialized
bool secDif = true;             // Indicates secBuf different from flash chip
uint32_t secIdx = 0;
uint32_t secBuf[1024] = {(uint32_t) -1 };
bool wrote_flash = false;       // Bool for testin (prevents unnecessary rewrites)


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
  // Set Interrupt Pins
  pinMode(VSPI_D2, INPUT);                    // CMD_RDY
  pinMode(VSPI_D3, OUTPUT);                   // D_RDY

  cmd_rdy = false;
  digitalWrite(VSPI_D3, HIGH);                // High means inactive, falling indicates ready
  attachInterrupt(VSPI_D2, cmd_isr, FALLING); // Attachs interrupt

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
    if (DEBUG) Serial.println("Error: Couldn't initialize NB as a SPI peripheral");
    return false;
  }
  if (DEBUG) Serial.println("SPI peripheral device initialized");
  return true;
}


// Reads data from given SD card file to Serial Monitor. Note: prefix filenames with '/'
void read_SD(const char filepath[]) {
  if (SD.exists(filepath)) {
    if (DEBUG) Serial.printf("%s:\n", filepath);
    File f = SD.open(filepath);
    while (f.available()) {
      if (DEBUG) Serial.write(f.read());
    }
    f.close();
    if (DEBUG) Serial.println("\n");
  } else {
    if (DEBUG) Serial.printf("Error: %s doesn't exist\n", filepath);
  }
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
  uint8_t dest = (cmd & 0b01110000);
  dest = dest >> 4;
  // Gets request size (0 is whole word, 1 is first half, 2 is second half, 3-6 are bytes 1-4)
  uint8_t size = (cmd & 0b00001110);
  size = size >> 1;
  // Gets if it's a read or write request ()
  bool write = (cmd & 0b00000001);
  ret.dest = dest;
  ret.size = size;
  ret.write = write;

  if (DEBUG) print_cmd_data(ret);
  
  return ret;
}

// Queues an SPI input transaction, the NB waiting on a command from the CPU
void create_input_queue(uint64_t* buf, int size) {
  memset(&message, 0, sizeof(message));
  message.flags = 0;      
  message.length = size;  // 8-bit cmd, 32-bit addr
  message.tx_buffer = (void*)NULL;
  message.rx_buffer = (void*)buf;
  message.user = (void*)1;

  // Queues message
  if (ESP_OK != spi_slave_queue_trans(cpu_host, &message, portMAX_DELAY)) {
    if (DEBUG) Serial.println("Error: couldn't request command");
  }
}

// Queues an SPI output transaction, the CPU getting sent data or write results
void create_output_queue(uint64_t* buf, int size) {
  memset(&message, 0, sizeof(message));
  message.flags = 0;      
  message.length = size;  // 32-bit addr read or write status
  message.tx_buffer = (void*)buf;
  message.rx_buffer = (void*)NULL;
  message.user = (void*)1;

  
  // Queues message
  if (ESP_OK != spi_slave_queue_trans(cpu_host, &message, portMAX_DELAY)) {
    if (DEBUG) Serial.println("Error: couldn't request command");
  }
  // send_ready();                         // For actual program, send trigger before transaciton
  // Serial.println("Data Ready");
}

// Gets NB-CPU transaction results
void wait_for_queue_results() {
  spi_slave_transaction_t* msg_rcv;  // Transaction struct
  if (ESP_OK != spi_slave_get_trans_result(cpu_host, (spi_slave_transaction_t**)&msg_rcv, portMAX_DELAY)) {
    if (DEBUG) Serial.println("Error: couldn't receive command");
  }
}

// Signals D_RDY so CPU knows we're ready to transmit
void send_ready() {
  digitalWrite(D_RDY, LOW);
  vTaskDelay(1);
  digitalWrite(D_RDY, HIGH);
}

// Waits until CPU triggers CMD_RDY
void get_ready() {
  while(cmd_rdy == false) {
    vTaskDelay(1);
    uint64_t i = 0;
    if (i++ > 1000) {
      if (DEBUG) Serial.println("Waiting for CMD_RDY");
      send_ready();
      if (DEBUG) Serial.println("Data Ready");
      i = 0;
    }
  }
  cmd_rdy = false;
}


// Consider deleting: same functionality with debug = true
void print_cmd_data(struct cmd_data data) {
  if (DEBUG) Serial.printf("dest: 0x%x\tsize: 0x%x\twrite: %d\n", data.dest, data.size, data.write);
}

// Clears 64-bit buffer (replacing with 0s)
void clear_buf(uint64_t* buf) {
  *buf = 0;
}

// Prints n-bit buffer in binary
void print_buf(uint64_t* buf, int n) {
  for (int j = n-1; j >= 0; j--) if (DEBUG) Serial.printf("%d",bitRead(*buf, j));
  if (DEBUG) Serial.printf("\n");
}


// Data from CPU arrives out of order, so this is needed to remix it
uint32_t unjumble(uint32_t d_i) {
  uint32_t bytes[4] = { 0 };
  for (int i = 0; i < 4; i++) {
    bytes[i] = (d_i & (0xFF<<8*i))>>8*i;
  }
  return (bytes[0]<<3*8) | (bytes[1]<<2*8) | (bytes[2]<<8) | (bytes[3]);
}


#define MUT_TIME 5000
uint32_t SB_reg = 0;
SemaphoreHandle_t SB_mut = NULL;
TaskHandle_t Task1;
// Handler for incoming SB transactions. Reads UART keyboard inputs into SB_reg buffer
// For debugging, takes in Serial input from NB
void SB_handler(void* parameter) {
  for (;;) {

    // while (SBuart.available() > 0) {
    while (Serial.available() > 0) {

      // uint32_t i = SBuart.read();
      uint32_t i = Serial.read();

      if (xSemaphoreTake(SB_mut, MUT_TIME)) {
        SB_reg = i;
        if (DEBUG) Serial.printf("SB: 0x%08x\n", SB_reg);
        xSemaphoreGive(SB_mut);
      }
    }
  }
}


// Setup initialization
void setup() {
  Serial.begin(115200);
  randomSeed(time(NULL));
  SBuart.begin(UART_BAUD);
  if (DEBUG) Serial.println("\nHSPI Init...");

  FL_init(10);

  if (DEBUG) Serial.printf("\nSD Init: %d\n", SD_init(0));

  init_spi(cpu_host, VSPI_CS, true);
  if (fencepost) {
    fencepost = false;
    if (DEBUG) Serial.println("Ready to receive cmd");
    create_input_queue(&cmd_rec, 32);
  }

  if (DEBUG) Serial.println("\nNorthbridge initialized");

  while (SB_mut == NULL) {
    SB_mut = xSemaphoreCreateMutex();
  }
  xTaskCreatePinnedToCore(SB_handler, "SB Handler", 1000, NULL, 0, &Task1, 0);
}


uint32_t sd_test = (uint32_t)-1;

// Main control loop
void loop() {
  if (xSemaphoreTake(SB_mut, MUT_TIME)) {
    // SB_keyboard = i++;
    if (DEBUG) Serial.printf("NB: 0x%08x\n", SB_reg);
    xSemaphoreGive(SB_mut);
  }  

  // if (!wrote_flash) {
  //   FL_clear();
  //   wrote_flash = true;
  //   for (uint32_t i = 0; i < 100; i++) {
  //     FL_printHexWord(4*i);
  //   }
  //   for (int i = 0; i < 100; i++) {
  //     uint32_t addr = 4*i;
  //     uint32_t data = i;
  //     FL_writeWord(addr, data, true);
  //   }
  
  // //   // flash.eraseSector(addr);
  // //   delay(2500);
  // //   Serial.println("About to write to flash");
    
  // //   // FL_printHexWord(addr);

  // //   // uint16_t data2 = 0xDEAD;
  // //   FL_writeWord(addr, 0x89ABCDEF, true);
  // //   // FL_printHexWord(addr);
   
  // //   delay(2500);

  // //   // FL_writeWord(addr2, 0xDEADBEEF, true);
  // //   // FL_printHexWord(addr2);

  // //   Serial.println("Done");
  // }

  // for (uint32_t i = 0; i < 100; i++) {
  //     FL_printHexWord(4*i);
  // }

  // Needs one dedicated core to service CPU SPI requests
  if (cmd_rdy) {
    
    cmd_rdy = false;
    send_ready();
    if (DEBUG) Serial.println("Data Ready");
    uint64_t i = 0;
    get_ready();
    wait_for_queue_results();  // Makes sure cmd is received

    if (DEBUG) Serial.print("\ncmd line: 0b");
    // cmd_rec &= 0xFF;
    for (int i = 31; i >= 0; i--) if (DEBUG) Serial.printf("%d", 1 && (cmd_rec & (1 << i)));
    if (DEBUG) Serial.println();

    struct cmd_data cmd = parse_cmd(cmd_rec, true);
    clear_buf(&cmd_rec);
    clear_buf(&rec_data);

    if (cmd.write) {                      // Write cmd needs addr & write payload
      create_input_queue(&rec_data, 64);
    } else {                              // Reads cmd just needs addr
      create_input_queue(&rec_data, 32);
    }
    cmd_rdy = false;
    send_ready();
    if (DEBUG) Serial.println("Data Ready");

    if (DEBUG) Serial.println("Waiting for addr");
    while (!cmd_rdy) vTaskDelay(100);
    wait_for_queue_results();
    payload = 0;
    addr = (uint32_t)(rec_data & 0xFFFFFFFF);
    addr = unjumble(addr);
    // if (cmd.write) {
      // payload = (uint32_t) (rec_data >> 32);
      // Serial.printf("addr: 0x%08x\t", addr);
      // Serial.printf("data: 0x%08x\n", payload);
    // } else {
      // print_buf((uint64_t*) &addr, 32);
      // Serial.printf("addr: 0x%08x\n", addr);
    // }

    uint32_t retWord = (uint32_t)-1;
    if (!cmd.write) {                 // write == 0 means reading
      if (DEBUG) Serial.println("Read command");
      if (DEBUG) Serial.printf("addr: 0x%08x\n", addr);

      // Implement read...
      switch(cmd.dest) {
        case PSRAM:
          if (DEBUG) Serial.println("The PSRAM read totally works...");
          break;
        case FLASH:
          if (DEBUG) Serial.println("The Flash read totally works...");
          retWord = FL_readWord(addr, true);

          if (DEBUG) Serial.printf("flash read: 0x%08x\n", retWord);

          break;
        case SD_CARD:
          if (DEBUG) Serial.printf("The SD read totally works...0x%08x\n", sd_test);
          retWord = sd_test;
          break;
        case SB:      // CPU reading SB's keyboard inputs
          if (DEBUG) Serial.printf("Reading from SB: ");
          if (xSemaphoreTake(SB_mut, MUT_TIME)) {
            if (DEBUG) Serial.printf("0x%08x\n", SB_reg);
            retWord = SB_reg;
            xSemaphoreGive(SB_mut);
          }
          break;
        case CPU_S:
        default:
          if (DEBUG) Serial.println("CPU reading from serial...? Shouldn't happen");
          break;
      }

      rec_data = unjumble(retWord);
      create_output_queue(&rec_data, 32);
      send_ready();
      if (DEBUG) Serial.println("Data Ready");
      // while (!cmd_rdy) vTaskDelay(1);
      wait_for_queue_results();
      
      // Send back data...
    } else {
      if (DEBUG) Serial.println("Write command");
      // Add control flow so only SB-targetted writes trigger send_MP3
      payload = (uint32_t) (rec_data >> 32);
      payload = unjumble(payload);
      // CPU sends data jumbled, so we need to unjumble it when we receive & jumble it when send
      if (DEBUG) Serial.printf("addr: 0x%08x\t", addr);
      if (DEBUG) Serial.printf("data: 0x%08x\n", payload);
      
      // bring_in_the_olives = true;
      uint32_t w_status = 0;
      switch(cmd.dest) {
        case PSRAM:
          if (DEBUG) Serial.println("The PSRAM write totally works...");
          break;
        case FLASH:
          if (DEBUG) Serial.println("The Flash write totally works...");
          FL_writeWord(addr, payload, true);
          break;
        case SD_CARD:
          sd_test = payload;
          retWord = payload;
          if (DEBUG) Serial.printf("The SD write totally works...0x%08x", sd_test);

          break;
        case SB:      // Triggers NB-CPU music transfer
          if (DEBUG) Serial.println("Write to SB... what?");
          break;
        case CPU_S:
        default:
          uint8_t byte = payload;
          srl_buf[srl_idx++] = byte;
          if (DEBUG) Serial.println("Added CPU serial output to buffer");
          bool endFound = false;
          for (uint32_t i = 0; i < SRL_MAX; i++) {
            if (srl_buf[i] == '\n') {
              endFound = true;
            }
          }
          if (endFound) {
            if (DEBUG) Serial.printf("CPU says: %s", srl_buf);
            memset(srl_buf, '\0', SRL_MAX);
            srl_idx = 0;
          }
          break;
      }
      send_ready();
      if (DEBUG) Serial.println("Data Ready");
    }

    // Currently sending test data. Implement actual read system using addr...
    clear_buf(&rec_data);

    cmd_rdy = false;

    if (DEBUG) Serial.println("Finished CPU transaction");

    clear_buf(&cmd_rec);
    create_input_queue(&cmd_rec, 32);
  }
  cmd_rdy = false;

  // // If RX'ed a CPU write command to SB, play music
  // bring_in_the_olives = true;
  // if (bring_in_the_olives) {
  //   bring_in_the_olives = false;
  //   send_MP3("/meglo.wav");
  //   delay(2500);
  // }
  // clear_buf(&cmd_rec);
  // create_input_queue(&cmd_rec, 32);

  delay(1000);
  Serial.println("Repeating main loop");
}
