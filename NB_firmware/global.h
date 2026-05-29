#ifndef GLOBAL_H
#define GLOBAL_H

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



// extern SoftwareSerial SBuart(UART_RX, UART_TX);
extern const uint32_t BUF_SIZE = 64;  // Bytes in tx/rx buffers

// For VSPI Configuration
extern spi_host_device_t cpu_host;
extern spi_bus_config_t spi_bus;
extern spi_slave_interface_config_t peripheral_config;
// extern spi_dma_chan_t dma_config = SPI_DMA_DISABLED;
extern int msg_idx;

// For UART Configuration
extern uart_config_t uart_config;
extern QueueHandle_t uart_queue;

// For HSPI Configuration
extern spi_host_device_t sd_host;
extern spi_bus_config_t sd_bus;
extern sdmmc_host_t sd_cfg = SDSPI_HOST_DEFAULT();
extern sdspi_device_config_t sd;
extern sdspi_dev_handle_t sd_handle;
extern spi_bus_config_t bus_cfg;
// extern SPIClass hspi(HSPI);
// extern SPIFlash flash(HSPI_CS_FL);
// extern SPIFlash psram(HSPI_CS_PS);  // Not yet working

extern uint64_t FL_MAX;
extern uint64_t PS_MAX;
extern uint32_t ret;
extern bool fencepost;
extern spi_slave_transaction_t message;  // Transaction struct



#endif
