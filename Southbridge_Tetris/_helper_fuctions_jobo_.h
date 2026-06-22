#if !defined(_helper_fuctions_jobo_)

#define _helper_fuctions_jobo_


#include <Arduino.h>

void printBinaryByte(uint8_t b, HardwareSerial* serial);
void printBinaryWord(uint32_t b, HardwareSerial* serial);
void clear_buf(uint8_t** buf_ptr, int length);
void shift_buffer_left(uint8_t* buf_ptr, int* length, int number_of_shifts);
void malloc_resize(uint8_t* buf_ptr, int* length, int size_change);


#endif //_helper_fuctions_jobo_