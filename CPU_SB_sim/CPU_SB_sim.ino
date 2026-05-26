/* Author: Damian Amerman-Smith
 * Testscript to simulate the CPU and SB for transmitting to the NB
 */
#include <Arduino.h>
#include <HardwareSerial.h>
#include <driver/spi_master.h>
#include <driver/uart.h>

#define VSPI_CLK 18
#define VSPI_CS 5
#define VSPI_MOSI 23    // D0
#define VSPI_MISO 19    // D1
#define VSPI_D2 21
#define VSPI_D3 22

#define UART_TX 16
#define UART_RX 17
#define UART_PORT UART_NUM_1
#define UART_BAUD 9600

const uint32_t BUF_SIZE = 64;
uint8_t TX_buf[BUF_SIZE+1];
static uint32_t RX_buf[64];
uint8_t UART_buf[BUF_SIZE+1];
uint8_t mp3_buf[1024];

// for SPI Configuration
spi_bus_config_t vspi;
spi_host_device_t host_config = SPI3_HOST; // Selecting SPI3: VSPI
spi_device_interface_config_t guest_config;
spi_device_handle_t guest_name;
spi_dma_chan_t dma_config = SPI_DMA_DISABLED;

// for UART Configuration
uart_config_t uart_config;
QueueHandle_t uart_queue;

bool d_rdy = false;
void IRAM_ATTR d_isr() {
  d_rdy = true;
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  memset(&vspi, 0, sizeof(vspi));
  vspi.sclk_io_num = VSPI_CLK;
  // vspi.data0_io_num = VSPI_MOSI;
  // vspi.data1_io_num = VSPI_MISO;
  // vspi.data2_io_num = VSPI_D2;
  // vspi.data3_io_num = VSPI_D3;


  vspi.flags = 0;
  vspi.max_transfer_sz = 4096;

  vspi.mosi_io_num = VSPI_MOSI;           // Bus Configuration
  vspi.miso_io_num = VSPI_MISO;
  vspi.quadwp_io_num = -1;
  vspi.quadhd_io_num = -1;
  // vspi.quadwp_io_num = VSPI_D2;
  // vspi.quadhd_io_num = VSPI_D3;
  
  if (ESP_OK != spi_bus_initialize(host_config, &vspi, dma_config)) {
    Serial.println("Error: Couldn't initialize SPI bus");
    return;
  }

  memset(&guest_config, 0, sizeof(guest_config));

  guest_config = {
    .command_bits = 0, 
    .address_bits = 0, 
    .dummy_bits = 0, 
    .mode = 0,
    .clock_speed_hz = 1 * 1000,
    .spics_io_num = VSPI_CS,
    .flags = 0,
    .queue_size = 3,
  };

  pinMode(VSPI_D2, OUTPUT);  // CMD_RDY
  pinMode(VSPI_D3, INPUT); // D_RDY

  d_rdy = false;
  digitalWrite(VSPI_D2, HIGH);    // High means inactive, falling indicates ready
  attachInterrupt(VSPI_D3, d_isr, FALLING);

  
  if (ESP_OK != spi_bus_add_device(host_config, &guest_config, &guest_name)) {
    Serial.println("Error: Couldn't add peripheral device");
    return;
  }
  Serial.println("\nCPU Initialized");

}



bool cpu_send_cmd(uint8_t cmd, uint32_t addr_i, uint32_t payload, bool debug) {
// To send data to peripheral, make a spi_transaction_t struct and
  // call spi_device_queue_trans/spi_device_get_trans_result or spi_device_transmit
  spi_transaction_t message;
  memset(&message, 0, sizeof(message));
  // uint8_t cmd_addr = cmd;

  static uint32_t buf = cmd;
  static uint64_t addr = ((uint64_t)addr_i<<32) | payload;
  

  message.flags = 0; //SPI_TRANS_USE_TXDATA
  message.length = 32;
  message.tx_buffer = (void*) &buf;
  message.rx_buffer = (void*) NULL;
  message.user = (void*) 1;
  
  d_rdy = false;
  send_ready();
  uint64_t i = 0;
  while (!d_rdy) {
    vTaskDelay(1);
    if (i++ > 1000) {
      Serial.println("Waiting for D_RDY");
      send_ready();
      i = 0;
    }
  }
  d_rdy = false;

  // SPI Transmission to send cmd word
  if (ESP_OK != spi_device_transmit(guest_name, &message)) {
    Serial.println("Error: Couldn't transmit message");
    return false;
  }
  send_ready();

  memset(&message, 0, sizeof(message));

  message.flags = 0; //SPI_TRANS_USE_TXDATA
  if (1 && (cmd & 0b00000011)) {  // write cmd, need to send payload data with address
    message.length = 64;
  } else {
    message.length = 32;
  }
  
  message.tx_buffer = (void*) &addr;
  message.rx_buffer = (void*) NULL;
  message.user = (void*) 1;

  // SPI Transmission to send addr word
  if (ESP_OK != spi_device_transmit(guest_name, &message)) {
    Serial.println("Error: Couldn't transmit message");
    return false;
  }

  if (debug) {
    Serial.print("cmd: 0b");
    for (int i = 31; i >= 0; i--) Serial.print(bitRead(buf, i));
    Serial.println("");
  

    Serial.print("addr: 0b");
    // Serial.print(addr, BIN);
    for (int i = 61; i >= 0; i--) Serial.print(bitRead(addr, i));
    
    Serial.println("");
  }

  // Serial.printf("Sent command to NB:");
  // for (int i = 31; i >= 0; i--) {
  //   Serial.print(bitRead(cmd_addr, i));
  // }
  // Serial.printf("\taddr: ");
  // 
  // Serial.printf("\n");
  return true;
}

// Signals CMD_RDY so NB knows we're ready to transmit
void send_ready() {
  digitalWrite(VSPI_D2, LOW);
  digitalWrite(VSPI_D2, HIGH);
}

// Simulation of one CPU memory/SB request
void loop() {
  Serial.printf("Sending cmd & addr: %d\n", cpu_send_cmd(0b0011011, 0xFF0000FF, 0x00FF00FF, true));
  // digitalWrite(VSPI_D2, LOW);
  // delay(10);
  // digitalWrite(VSPI_D2, HIGH);

  delay(20);

  // if (d_rdy) {
  //   Serial.println("Getting D_RDY trigger");
  //   uint32_t buf = 0;
  //   memset(&buf, 0, sizeof(buf));

  //   spi_transaction_t message;
  //   memset(&message, 0, sizeof(message));

  //   // Serial.println("before flags");
  //   message.flags = 0; //SPI_TRANS_MODE_DIO
  //   message.length = 32;
  //   message.rxlength = 32;
  //   // message.mode = 
  //   message.tx_buffer = (void*) NULL;
  //   message.rx_buffer = (void*) &buf;

  //   message.user = (void*) 1;
    
  //   // SPI Receipt
  //   if (ESP_OK != spi_device_transmit(guest_name, &message)) {
  //     Serial.println("Error: Couldn't transmit message");
  //     return;
  //   }

  //   Serial.printf("SPI NB: 0b");
  //   for (uint8_t i = 31; i > 0; i--) Serial.printf("%d", 1 && (buf & (1 << i)));

  //   // Serial.print(buf, BIN);
  //   Serial.println("");

  //   d_rdy = false;
  // }
  delay(2500);
}
