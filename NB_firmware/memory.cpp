/*****************************************************************************
 * Author: Damian Amerman-Smith
 * Helper functions for memory I/O, including Flash, SD card, and PSRAM access.
 *****************************************************************************/
// Libraries
#include <Arduino.h>
#include <stdint.h>
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

extern SPIClass hspi;
extern SPIFlash flash;
extern SPIFlash psram;  // Not yet working

/*****************************************************************************/
// 

// SD card initialization. Timeout is number of seconds to wait (it'll try every 2.5 sec)
bool SD_init(double timeout) {
  SPI.begin(HSPI_CLK, HSPI_MISO, HSPI_MOSI, HSPI_CS_SD);
  double elapsed = 0;
  if (DEBUG) Serial.printf("SD Initing...\n");

  while (!SD.begin(HSPI_CS_SD, SPI) & (elapsed < timeout)) {  // Tries to connect for 2.5 seconds
    if (DEBUG) Serial.println("Error: SD Card mount failed");
    delay(2500);
    elapsed += 2.5;
  }
  uint8_t sd_type = SD.cardType();
  if (sd_type == CARD_NONE) {
    if (DEBUG) Serial.println("Error: no SD Card attached");
    return false;
  }
  if (DEBUG) Serial.printf("\nSD Card size: %d\nSectors: %d\nTotal: %d Bytes\nUsed: %d Bytes\n\n", SD.cardSize(), SD.numSectors(), SD.sectorSize(), SD.totalBytes(), SD.usedBytes());
  return true;
}

// Gets SD card size
size_t get_SD_size(const char filepath[]) {
  if (SD.exists(filepath)) {
    File f = SD.open(filepath);
    size_t size = f.size();
    return size;
  } else {
    if (DEBUG) Serial.printf("Error: %s doesn't exist\n", filepath);
    return 0;
  }
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

// Reads data from given SD card file to SB using UART. Note: buffer is passed by reference,
// and must be instantiated with the required file's size before calling this function
// Note: UART bottlenecked old setup from being able to use UART for continuous WAV transmission
size_t read_SD_to_SB(const char filepath[], size_t size, uint8_t* buf) {
  if (SD.exists(filepath)) {
    File f = SD.open(filepath);
    size_t sent = 0;
    bool hit_data = false;
    while (f.available()) {
      // Add compression after data header 'd''a''t''a'
      //  (aside from header, drop two bytes every other two bytes)
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
    if (DEBUG) Serial.println("exiting read_SD2buf");
    return sent;
  } else {
    if (DEBUG) Serial.printf("Error: %s doesn't exist\n", filepath);
    return 0;
  }
}

// Reads an ~~mp3~~ (currently only WAV) file to a byte steam, then sends it to the Southbridge via UART.
// Buffer will be internally created and is deleted before the end of the function
// Will return status: 0 is failure, otherwise number of bytes sent
size_t send_MP3(const char filepath[]) {
  if (DEBUG) Serial.println("send_MP3()");
  if (SD.exists(filepath)) {
    size_t size = get_SD_size(filepath);  // Opens file
    size_t sent = 0;
    if (DEBUG) Serial.printf("File %s is: ", filepath);
    if (DEBUG) Serial.print(size);
    if (DEBUG) Serial.print(" bytes. Need ");
    if (DEBUG) Serial.print(size / TRAN_SIZE);
    if (DEBUG) Serial.println(" transmissions");

    while (sent < size) {   // Note: this may not need to be a loop
      uint8_t* buf = NULL;  // Allocates buffer
      buf = (uint8_t*)heap_caps_malloc(TRAN_SIZE + 1, MALLOC_CAP_8BIT);
      if (NULL == buf) {
        if (DEBUG) Serial.println("failed to make buffer");
        return 0;
      }
      memset(buf, '\0', TRAN_SIZE + 1);
      sent += read_SD_to_SB(filepath, size, (uint8_t*)buf);  // Reads file to buffer
      if (DEBUG) Serial.println("");
      heap_caps_free((void*)buf);
    }

    if (DEBUG) Serial.println("sent mp3");

    return size;
  } else {
    if (DEBUG) Serial.printf("Error: %s not found\n", filepath);
    return 0;
  }
}
/*****************************************************************************/
// Flash Functions
// Flash initialization
bool FL_init(double timeout) {
  SPI.begin(HSPI_CLK, HSPI_MISO, HSPI_MOSI, HSPI_CS_FL);
  SPI.setDataMode(SPI_MODE0);
  ret = 0;
  uint8_t i = 0;
  while((ret == 0) && ((0.25)*(double)i++ < timeout)) {
    ret = flash.begin(16*1000*1000);
    delay(250);
    if (DEBUG) Serial.println("Trying to init...");
  }
  if (DEBUG) Serial.printf("Flash Init: %d\tJEDEC ID: 0x%x\t", ret, flash.getJEDECID());
  FL_MAX = flash.getCapacity();
  if (DEBUG) Serial.printf("%d kB Capacity\n", FL_MAX / 1000);
  for (uint32_t i = 0; i < 1024; i++) {
    secBuf[i] = (uint32_t)-1;
  }
  secInit = false;
  return ret;
}

// Wipes entire flash chip. Be cautious about using this function
bool FL_clear() {
  if (!flash.eraseChip()) {
    if (DEBUG) Serial.println("Error: Couldn't clear Flash Memory");
    return false;
  }
  return true;
}

// Prints one word in hex values
uint32_t FL_printHexWord(uint32_t addr) {
  if (addr % 4 ) {            // Ensures addr is word-aligned
    return 0;
  }
  uint32_t data = -1;
  ret = 0;
  data = FL_readWord(addr, false);
  if (DEBUG) Serial.printf("FL:\t0x%x:\t0x%x\n", addr, data);
  return data;
}

// Prints serial bytes as hex values
void FL_printHex(uint32_t start, uint32_t end) {
  uint8_t test[end - start + 1] = { 0 };
  if (DEBUG) Serial.printf("Reading from flash:\n");
  ret = 0;
  ret = flash.readByteArray(start, (uint8_t*)&test, end - start, true);
  if (!ret) {
    if (DEBUG) Serial.println("Error: Couldn't read flash");
    return;
  }
  for (uint32_t i = start; i < end; i++) {
    if ((i % (0xFF + 1)) == 0) {
      if (DEBUG) Serial.printf("\n0x%x\t", i);
    }
    if (DEBUG) Serial.printf("%x", test[i]);
  }
  if (DEBUG) Serial.printf("\nEnd read\n");
}

// Prints serial bytes as characters
void FL_printChar(uint32_t start, uint32_t end) {
  uint8_t test[end - start] = { 0 };
  if (DEBUG) Serial.printf("Reading from flash:\n");
  ret = 0;
  ret = flash.readByteArray(start, (uint8_t*)&test, (end - start), true);
  if (!ret) {
    if (DEBUG) Serial.println("Error: Couldn't read flash");
    return;
  }
  for (uint32_t i = start; i < end; i++) {
    if ((i % (0xFF + 1)) == 0) {
      if (DEBUG) Serial.printf("\n0x%x\t", i);
    }
    if (DEBUG) Serial.printf("%c", test[i]);
  }
  if (DEBUG) Serial.printf("\nEnd read\n");
}

// Erases sector, then write buffer to said sector
bool FL_flush(uint32_t newSecIdx, bool debug) {
  if (!secInit || !secDif) return true;
  // Ensures data is zeroed out before writing buffer
  bool ok2 = false;
  uint32_t attempt2 = 0;
  while (!ok2 && (attempt2++ < (uint32_t)-1)) {
    ok2 = flash.eraseSector(secIdx);
    delay(5);
  }
  if (debug) Serial.printf("Flush needs to rewrite sector 0x%x\n", secIdx);
  for (uint32_t i = 0; i < 1024; i++) {
    flash.writeULong(secIdx + 4*i, secBuf[i]);
  }
  if (debug) Serial.println("Flushed");
  secIdx = newSecIdx;
  for (uint32_t i = 0; i < 1024; i++) {
    secBuf[i] = flash.readULong(secIdx + 4*i);
  }
  // if (debug) Serial.printf("Erase sector 0x%x\n", secIdx);
  // for (uint32_t i = 0; i < 1024; i++) {
  //   bool ok = false;
  //   uint32_t attempt = 0;
  //   while (!ok && (attempt++ < (uint32_t)-1)) {
  //     ok = flash.writeULong(secIdx+4*i, secBuf[i], false);
  //     delay(5);
  //   }
  //   if (!ok) {
  //     Serial.printf("Write failed at 0x%x\n", secIdx+4*i);
  //     return false;
  //   }
  //   delay(2);
  //   if (debug) Serial.print(".");
  // }
  // if (debug) Serial.println();
  secDif = false;
  return true;
}

// 32-bit Write
bool FL_writeWord(const uint32_t addr, uint32_t data, bool debug) {
  // Flash memory does erases (changing 0s to 1s) on a sector-level (4 kB)
  // In order to allow word-level writes, we will need to see if we have to
  // erase the whole sector and if so, rewrite all but the new word
  if (DEBUG) Serial.printf("FL_writeWord: addr 0x%08x\tdata 0x%08x\n", addr, data);
  if (addr % 4 ) {            // Ensures addr is word-aligned
    return false;
  }
  uint32_t newSecIdx = (addr & ~(0xFFF));   // Sector addr removes lowest 12 bits
  if (newSecIdx != secIdx) {              // Doing writes on a different sector
    FL_flush(newSecIdx, debug);         // Writes buffered changes
    secInit = true;
    secIdx = newSecIdx; 
    for (uint32_t i = 0; i < 1024; i++) {// Fills buffer with new sector
      secBuf[i] = flash.readULong(secIdx + 4*i);
    }
  }
  secBuf[(addr & 0xFFF) >> 2] = data;

  if (DEBUG) Serial.println("Reaching end");  
  secDif = true;
  return true;
}

// 32-bit Read
uint32_t FL_readWord(uint32_t addr, bool debug) {
  if (addr % 4 ) {            // Ensures addr is word-aligned (returning null if not)
    return (uint32_t)-1;
  }
  if (secInit && ((addr & ~0xFFF) == secIdx)) {
    FL_flush(addr & ~0xFFF, debug);
    if (debug) Serial.printf("FL at 0x%x: 0x%x\n", addr, secBuf[(addr & 0xFFF) >> 2]);
    return secBuf[(addr & (0xFFF)) >> 2];
  }
  FL_flush((addr & ~0xFFF), false);

  for (uint32_t i = 0; i < 1024; i++) {
    secBuf[i] = flash.readULong((secIdx) + 4*i);
  }
  secInit = true;
  uint32_t data = secBuf[(addr & 0xFFF) >> 2];
  // uint64_t attempts = 0;
  // uint32_t newdata = 1;
  // // bool ok = false;
  // while (attempts++ < (uint64_t)-1) {
  //   newdata = flash.readULong(addr);
  //   if (data == newdata) {
  //     break;
  //   }
  //   data = newdata;
  // }
  if (debug) Serial.printf("FL at 0x%x: 0x%x\n", addr, data);
  return data;
}

// 16-bit Write
bool FL_writeShort(uint32_t addr, uint16_t data, uint8_t position, bool debug) {
  if ((addr % 4 ) || ((position != 0b001) && (position != 0b010)))  return false;
  // Need to grab entire word to overwrite desired portion
  uint32_t oldData = FL_readWord(addr, debug);
  if (debug) Serial.printf("Writing 0x%x to 0x%x\n", data, oldData);
  if (position == 0b001) {                      // Writing to first short (MSB)
    oldData &= 0xFFFF;
    oldData |= (data << 16);
  } else if (position == 0b010) {               // Writing to second short
    oldData &= ~0xFFFF;
    oldData |= data;
  }
  if (debug) Serial.printf("Wrote: 0x%x\n", oldData);
  secBuf[(addr & 0xFFF) >> 2] = oldData;
  secDif = true;
  while (!FL_writeWord(addr, oldData, debug));
  FL_flush(addr & ~0xFFF, false);
  return true;
}

// 16-bit Read
uint16_t  FL_readShort(uint32_t addr, uint8_t position, bool debug) {
  if ((addr % 4 ) || ((position != 0b001) && (position != 0b010)))  return (uint16_t)-1;
  uint32_t data = FL_readWord(addr, true);
  uint16_t payload;
  if (position == 0b001) {
    data &= ~0xFFFF;
    payload = data >> 16;
  } else if (position == 0b010) {
    data &= 0xFFFF;
    payload = data;
  }
  if (debug) Serial.printf("FL at 0x%x, pos %d: 0x%x\n", addr, position, payload);
  return payload;
}

// 8-bit Write
bool FL_writeByte(uint32_t addr, uint8_t data, uint8_t position, bool debug) {
  if ((addr % 4 ) || ((position != 0b011) && (position != 0b100)
   && (position != 0b101) && (position != 0b110)))  return false;
  // Need to grab entire word to overwrite desired portion
  uint32_t oldData = FL_readWord(addr, false);
  if (position == 0b011) {
    oldData &= 0xFFFFFF;
    oldData |= data << 3*8;
  } else if (position == 0b100) {
    oldData &= 0xFF00FFFF;
    oldData |= data << 2*8;
  } else if (position == 0b101) {
    oldData &= 0xFFFF00FF;
    oldData |= data << 8;
  } else if (position == 0b110) {
    oldData &= 0xFFFFFF00;
    oldData |= data;
  }
  if (debug) Serial.printf("Wrote: 0x%x\n", oldData);
  secBuf[(addr & 0xFFF) >> 2] = oldData;
  secDif = true;
  while (!FL_writeWord(addr, oldData, debug));
  FL_flush(addr & ~0xFFF, false);
  return true;
}

// 8-bit Read
uint8_t FL_readByte(uint32_t addr, uint8_t position, bool debug) {
  if ((addr % 4 ) || ((position != 0b011) && (position != 0b100)
   && (position != 0b101) && (position != 0b110))) return (uint8_t)-1;
  uint32_t data = FL_readWord(addr, false);
  uint8_t payload;
  if (position == 0b011) {
    payload = (data & 0xFF000000) >> 3*8;
  } else if (position == 0b100) {
    payload = (data & 0xFF0000) >> 2*8;
  } else if (position == 0b101) {
    payload = (data & 0xFF00) >> 8;
  } else if (position == 0b110) {
    payload = data & 0xFF;
  }
  if (debug) Serial.printf("FL at 0x%x, pos %d: 0x%x\n", addr, position, payload);
  return payload;
}
/*****************************************************************************/