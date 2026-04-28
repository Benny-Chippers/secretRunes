/* Author: Damian Amerman-Smith
 * Testscript to simulate the CPU and SB for transmitting to the NB
 */


#include <Arduino.h>
#include <HardwareSerial.h>
#include <driver/spi_master.h>
#include <driver/uart.h>

#define VSPI_CS 5
#define VSPI_CLK 18
#define VSPI_MOSI 23    // D0
#define VSPI_MISO 19    // D1

#define UART_TX 16
#define UART_RX 17
#define UART_PORT UART_NUM_1
#define UART_BAUD 9600

const uint32_t BUF_SIZE = 512;
uint8_t TX_buf[BUF_SIZE];
uint8_t RX_buf[BUF_SIZE];
uint8_t UART_buf[BUF_SIZE];
uint8_t mp3_buf[1024];

// for SPI Configuration
spi_bus_config_t vspi;
spi_host_device_t host_config = SPI2_HOST; // Selecting SPI3: VSPI
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
  vspi.mosi_io_num = VSPI_MOSI;           // Bus Configuration
  vspi.miso_io_num = VSPI_MISO;
  vspi.quadwp_io_num = -1;
  vspi.quadhd_io_num = -1;
  vspi.max_transfer_sz = 4094;

  if (ESP_OK != spi_bus_initialize(host_config, &vspi, dma_config)) {
    Serial.println("Error: Couldn't initialize SPI bus");
    return;
  }

  guest_config = {
    .command_bits = 0,          // should be 6
    .address_bits = 0,         // should be 26
    .dummy_bits = 0,
    .mode = 0,
    .clock_speed_hz = 1*1000,
    .spics_io_num = VSPI_CS,
    .queue_size = 10,
  };
  
  if (ESP_OK != spi_bus_add_device(host_config, &guest_config, &guest_name)) {
    Serial.println("Error: Couldn't add peripheral device");
    return;
  }
  Serial.println("\nCPU Initialized");

  uart_config = {
    .baud_rate = UART_BAUD,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
  };
  
  if (ESP_OK != uart_driver_install(UART_PORT, BUF_SIZE, BUF_SIZE, 10, &uart_queue, 0)) {
    Serial.println("Error: Couldn't add UART");
  }
  if (ESP_OK != uart_param_config(UART_PORT, &uart_config)) {
    Serial.println("Error: Couldn't add UART");
  }
  if (ESP_OK != uart_set_pin(UART_PORT, UART_TX, UART_RX, 0, 0)) {
    Serial.println("Error: Couldn't add UART");
  }

  Serial.println("Southbridge Initialized");
  while (1) {
    // Tests NB->SB Communication
    memset(mp3_buf, '\0', 1024);
    uart_read_bytes(UART_PORT, (char*) mp3_buf,  1024, 10000000);
    
    Serial.printf("Incoming data: ");
    Serial.println((char*) mp3_buf);
  }
}

void loop() {
  // memset(TX_buf, '\0', BUF_SIZE);  // Prepares communication buffers
  // memset(RX_buf, '\0', BUF_SIZE);
  // strcpy((char*) TX_buf, "This is a SPI message from the CPU to the NB.");
  // // // strcpy((char*) RX_buf, "                               ");

  // // To send data to peripheral, make a spi_transaction_t struct and
  // // call spi_device_queue_trans/spi_device_get_trans_result or spi_device_transmit
  // spi_transaction_t message;
  // memset(&message, 0, sizeof(message));

  // message.flags = 0;
  // message.length = 512;
  // message.tx_buffer = (void*) TX_buf;
  // message.rx_buffer = (void*) RX_buf;
  // message.cmd = 0b00110000;
  // message.addr = 0x00FF00FF00;
  // message.user = (void*) 1;

  // Serial.print("cmd: 0b");
  // Serial.print(0x0F, BIN);
  // Serial.println("");
  // Serial.print("addr: 0b");
  // Serial.print(0x00FF00FF00, BIN);
  // Serial.println("");
  

  // // SPI Transmission
  // if (ESP_OK != spi_device_transmit(guest_name, &message)) {
  //   Serial.println("Error: Couldn't transmit message");
  //   return;
  // }
  
  // // if (strcmp((char*) RX_buf, "This is a SPI message from the CPU to the NB.")) {
  // Serial.print("SPI NB: ");
  // Serial.println((char*)RX_buf);
  // // }
  // // Serial.println((char*)TX_buf);


  // Tests SB->NB Communication
  // memset(UART_buf, '\0', BUF_SIZE);
  // uart_read_bytes(UART_PORT, UART_buf, BUF_SIZE - 1, 1000);    
  // Serial.print("UART NB: ");
  // Serial.println((char*) UART_buf);
  // uart_flush(UART_PORT);
  // delay(100);

  // strcpy((char*) UART_buf, "This is a UART message from the SB to the NB. \n");

  // delay(800);
}
