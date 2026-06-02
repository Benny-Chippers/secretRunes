#include <stdint.h>
/* Author: Damian Amerman-Smith
 * Helper functions for memory I/O, including Flash, SD card, and PSRAM access.
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

#include "memory.h"
#include "constants.h"
#include "config.h"
#include "global.h"

extern SPIClass hspi;
extern SPIFlash flash;
extern SPIFlash psram;  // Not yet working



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

// Prints one word in hex values
uint32_t FL_printHexWord(uint32_t addr) {
  if (addr % 4 ) {            // Ensures addr is word-aligned
    return 0;
  }
  uint32_t data = -1;
  ret = 0;
  data = FL_readWord(addr, false);
  Serial.printf("FL:\t0x%x:\t0x%x\n", addr, data);
  return data;
}

// Prints serial bytes as hex values
void FL_printHex(uint32_t start, uint32_t end) {
  uint8_t test[end - start + 1] = { 0 };
  Serial.printf("Reading from flash:\n");
  ret = 0;
  ret = flash.readByteArray(start, (uint8_t*)&test, end - start, true);
  if (!ret) {
    Serial.println("Error: Couldn't read flash");
    return;
  }
  for (uint32_t i = start; i < end; i++) {
    if ((i % (0xFF + 1)) == 0) {
      Serial.printf("\n0x%x\t", i);
    }
    Serial.printf("%x", test[i]);
  }
  Serial.printf("\nEnd read\n");
}

// Prints serial bytes as characters
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
    Serial.printf("%c", test[i]);
  }
  Serial.printf("\nEnd read\n");
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




// Flash memory does erases (changing 0s to 1s) on a sector-level
// In order to allow word-level writes, we will need to see if we have to
// erase the whole sector and if so, rewrite all but the new word
bool FL_writeWord(const uint32_t addr, uint32_t data, bool debug) {
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

  secDif = true;
  return true;
}


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

