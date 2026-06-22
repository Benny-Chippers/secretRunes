
#include "Parser.h"
#include <Arduino.h>

uint32_t keyboard_parse(uint8_t* data) {
  Serial.println("Parsing keyboard");
  uint32_t ret = data[2] | (data[3] << 8) | (data[4] << 16) | (data[5] << 24); 
  Serial.println("post ret dec");
  ret = ret & 0x3FFFFFFF;   //get rid of top 2 bits
  ret = ret >> 4;           //shift out non characters
  uint32_t com_per = (data[8] & 0b11000000);
  com_per = com_per >> 6; 
  com_per = com_per << 26; 
  return ret | com_per;
}

