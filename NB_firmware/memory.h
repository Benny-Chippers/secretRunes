/* Author: Damian Amerman-Smith
 * Function declarations for memory I/O, including Flash, SD card, and PSRAM access.
 */

#ifndef MEMORY_H
#define MEMORY_H

// Flash I/O Functions
bool      FL_clear();
uint32_t  FL_printHexWord(uint32_t addr);
bool      FL_flush(bool debug);
bool      FL_writeWord(const uint32_t addr, uint32_t data, bool debug);
uint32_t  FL_readWord(uint32_t addr, bool debug);
bool      FL_writeShort(uint32_t addr, uint16_t data, uint8_t position, bool debug);
uint16_t  FL_readShort(uint32_t addr, uint8_t position, bool debug);
bool      FL_writeByte(uint32_t addr, uint8_t data, uint8_t position, bool debug);
uint8_t   FL_readByte(uint32_t addr, uint8_t position, bool debug);



void FL_printHex(uint32_t start, uint32_t end);
void FL_printChar(uint32_t start, uint32_t end);



bool init_SD(double timeout);

#endif
