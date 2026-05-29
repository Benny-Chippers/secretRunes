



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

#include "memory.h"
#include "constants.h"
#include "config.h"
#include "global.h"

extern SPIClass hspi(HSPI);
extern SPIFlash flash(HSPI_CS_FL);
extern SPIFlash psram(HSPI_CS_PS);  // Not yet working



// Timeout is number of seconds to wait (it'll try every 2.5 sec)
bool init_SD(double timeout) {
  SPI.begin(HSPI_CLK, HSPI_MISO, HSPI_MOSI, HSPI_CS_SD);
  double elapsed = 0;
  while (!SD.begin(HSPI_CS_SD, SPI) & (elapsed < timeout)) {  // Tries to connect for 2.5 seconds
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

uint32_t FL_printHex32(uint32_t start) {
  uint32_t test = 0;
  Serial.printf("Reading from flash:\n");
  ret = 0;
  // for (uint32_t i = start; i < end; i++) {
  test = flash.readULong(start, true);
  Serial.printf("%x", (uint32_t)test);
  Serial.printf("\nEnd read\n");
  return test;
}

void FL_printHex(uint32_t start, uint32_t end) {
  uint8_t test[end - start + 1] = { 0 };
  Serial.printf("Reading from flash:\n");
  ret = 0;
  ret = flash.readByteArray(start, (uint8_t*)&test, end, true);
  if (!ret) {
    Serial.println("Error: Couldn't read flash");
    return;
  }
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