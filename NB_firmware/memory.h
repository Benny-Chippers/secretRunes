/* Author: Damian Amerman-Smith
 * Function declarations for memory I/O, including Flash, SD card, and PSRAM access.
 */

#ifndef MEMORY_H
#define MEMORY_H

// Flash I/O Functions
bool FL_clear();
uint32_t FL_printHex32(uint32_t start);
void FL_printHex(uint32_t start, uint32_t end);
void FL_printChar(uint32_t start, uint32_t end);



bool init_SD(double timeout);

#endif
