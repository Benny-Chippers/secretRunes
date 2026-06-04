/* Author: Damian Amerman-Smith
 * Function declarations for memory I/O, including Flash, SD card, and PSRAM access.
 */

#ifndef MEMORY_H
#define MEMORY_H

/*****************************************************************************/
// Defined constants for CPU memory requests
// Destination  
#define PSRAM   0b000
#define FLASH   0b001
#define SD_CARD 0b010
#define SB      0b011
#define CPU_S   0b100   // Serial Output from CPU

// Size
#define WORD    0b000   // Whole word
#define FSHORT  0b001   // First short
#define LSHORT  0b010   // Last short
#define FBYTE   0b011   // First byte
#define SBYTE   0b100   // Second byte
#define TBYTE   0b101   // Third byte
#define LBYTE   0b110   // Last byte

/*****************************************************************************/
// SD Card I/O Functions
bool SD_init(double timeout);
size_t get_SD_size(const char filepath[]);
size_t read_SD_to_SB(const char filepath[], size_t size, uint8_t* buf);
size_t send_MP3(const char filepath[]);

/*****************************************************************************/
// Flash I/O Functions
bool      FL_clear();
uint32_t  FL_printHexWord(uint32_t addr);
bool      FL_flush(uint32_t newSecIdx, bool debug);
bool      FL_writeWord(const uint32_t addr, uint32_t data, bool debug);
uint32_t  FL_readWord(uint32_t addr, bool debug);
bool      FL_writeShort(uint32_t addr, uint16_t data, uint8_t position, bool debug);
uint16_t  FL_readShort(uint32_t addr, uint8_t position, bool debug);
bool      FL_writeByte(uint32_t addr, uint8_t data, uint8_t position, bool debug);
uint8_t   FL_readByte(uint32_t addr, uint8_t position, bool debug);

void FL_printHex(uint32_t start, uint32_t end);
void FL_printChar(uint32_t start, uint32_t end);

/*****************************************************************************/


#endif
