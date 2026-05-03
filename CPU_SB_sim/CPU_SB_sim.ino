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

const uint32_t BUF_SIZE = 512/8;
uint8_t TX_buf[BUF_SIZE];
uint8_t RX_buf[BUF_SIZE];
uint8_t UART_buf[BUF_SIZE];
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
  vspi.quadwp_io_num = VSPI_D2;
  vspi.quadhd_io_num = VSPI_D3;
  
  vspi.max_transfer_sz = 4094;

  if (ESP_OK != spi_bus_initialize(host_config, &vspi, dma_config)) {
    Serial.println("Error: Couldn't initialize SPI bus");
    return;
  }

  memset(&guest_config, 0, sizeof(guest_config));

  guest_config = {
    .command_bits = 6,          // should be 6
    .address_bits = 26,         // should be 26
    .dummy_bits = 0,
    .mode = 3,
    .clock_speed_hz = 1*10000,
    .spics_io_num = VSPI_CS,
    .flags = SPI_DEVICE_HALFDUPLEX,
    // .flags = 0,
    .queue_size = 1024,
  };
  
  if (ESP_OK != spi_bus_add_device(host_config, &guest_config, &guest_name)) {
    Serial.println("Error: Couldn't add peripheral device");
    return;
  }
  Serial.println("\nCPU Initialized");

}

void loop() {
  memset(TX_buf, '\0', BUF_SIZE);  // Prepares communication buffers
  memset(RX_buf, '\0', BUF_SIZE);
  
  // memset((TX_buf), 0xF, 4);
  
  
  memset(RX_buf, '\0', BUF_SIZE);
  strcpy((char*) TX_buf, "f f f x x x ");
  // strcpy((char*) RX_buf, "hey rx    ");

  // To send data to peripheral, make a spi_transaction_t struct and
  // call spi_device_queue_trans/spi_device_get_trans_result or spi_device_transmit
  spi_transaction_t message;
  memset(&message, 0, sizeof(message));

  // Serial.println("before flags");
  message.flags = SPI_TRANS_MODE_QIO;
  message.length = 512;
  // message.mode = 
  message.tx_buffer = (void*) &TX_buf;
  message.rx_buffer = (void*) NULL;
  message.cmd = 0b00110000;
  message.addr = 0x00FF00FF00;
  message.user = (void*) 1;

  
  // SPI Transmission
  if (ESP_OK != spi_device_transmit(guest_name, &message)) {
    Serial.println("Error: Couldn't transmit message");
    return;
  }
  
  // // if (strcmp((char*) RX_buf, "This is a SPI message from the CPU to the NB.")) {
  Serial.print("SPI NB: ");
  Serial.printf("%s\n", (char*)RX_buf);
  // // }
  // Serial.println((char*)TX_buf);


  // Tests SB->NB Communication
  // memset(UART_buf, '\0', BUF_SIZE);
  // uart_read_bytes(UART_PORT, UART_buf, BUF_SIZE - 1, 1000);    
  // Serial.print("UART NB: ");
  // Serial.println((char*) UART_buf);
  // uart_flush(UART_PORT);
  // delay(100);

  // strcpy((char*) UART_buf, "This is a UART message from the SB to the NB. \n");

  delay(2000);
}
