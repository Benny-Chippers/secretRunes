
#include "_helper_fuctions_jobo_.h"
#include <Arduino.h>



void printBinaryByte(uint8_t b, HardwareSerial* serial) {
  for (int i = 7; i >= 0; i--) {
    serial->print(bitRead(b, i));
  }
  serial->println();
}


void printBinaryWord(uint32_t b, HardwareSerial* serial) {
  serial->println("printing bin");
  for (int i = 31; i >= 0; i--) {
    serial->print(bitRead(b, i));
  }
  serial->println();
}

void clear_buf(uint8_t** buf_ptr, int length) {
  memset(*buf_ptr, 0, length);
}

void shift_buffer_left(uint8_t* buf_ptr, int* length, int number_of_shifts) {
  for(int i = 0; i < *length - number_of_shifts; i++) {
    buf_ptr[i] = buf_ptr[i + number_of_shifts];
  }
  for(int i = *length - number_of_shifts; i < *length; i++) {
    buf_ptr[i] = 0;
  }
  *length -= number_of_shifts;
}


void malloc_resize(uint8_t** buf_ptr, int* length, int size_change) {
  int new_len = *length + size_change;
  uint8_t* temp = (uint8_t*)heap_caps_malloc(new_len, MALLOC_CAP_8BIT);
  memset(temp, 0, new_len);
  memcpy(temp, *buf_ptr, new_len);
  free(*buf_ptr);
  buf_ptr = &temp;
  *length = new_len; 
}