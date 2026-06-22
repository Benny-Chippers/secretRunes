
#include "max3421e.h"
#include "_helper_fuctions_jobo_.h"
#include <Arduino.h>
#include <SPI.h>
#include <esp_heap_caps.h>

max3421e_SPI::max3421e_SPI(SPIClass* spi) {
  _mosi = MOSI;
  _miso = MISO;
  _sclk = SCLK;
  _cs = CS;
  _gpx = GPX;
  _int = INT;
  _rst = RST;
  _spi = spi;
  _spi_setting = new SPISettings(SPI_SPEED, MSBFIRST, SPI_MODE0);
}

max3421e_SPI::max3421e_SPI(int spi_speed, SPIClass* spi) {
  _mosi = MOSI;
  _miso = MISO;
  _sclk = SCLK;
  _cs = CS;
  _gpx = GPX;
  _int = INT;
  _rst = RST;
  _spi = spi;
  _spi_setting = new SPISettings(spi_speed, MSBFIRST, SPI_MODE0);
}

max3421e_SPI::max3421e_SPI(uint8_t mosi_pin, uint8_t miso_pin, uint8_t sclk_pin, 
                    uint8_t cs_pin, uint8_t gpx_pin, uint8_t int_pin, uint8_t rst_pin, SPIClass* spi) {
  _mosi = mosi_pin;
  _miso = miso_pin;
  _sclk = sclk_pin;
  _cs = cs_pin;
  _gpx = gpx_pin;
  _int = int_pin;
  _rst = rst_pin;
  _spi = spi;
  _spi_setting = new SPISettings(SPI_SPEED, MSBFIRST, SPI_MODE0);
}

max3421e_SPI::max3421e_SPI(uint8_t mosi_pin, uint8_t miso_pin, uint8_t sclk_pin, 
                    uint8_t cs_pin, uint8_t gpx_pin, uint8_t int_pin, uint8_t rst_pin, int spi_speed, SPIClass* spi) {
  _mosi = mosi_pin;
  _miso = miso_pin;
  _sclk = sclk_pin;
  _cs = cs_pin;
  _gpx = gpx_pin;
  _int = int_pin;
  _rst = rst_pin;
  _spi = spi;
  _spi_setting = new SPISettings(spi_speed, MSBFIRST, SPI_MODE0);
}

void max3421e_SPI::begin() {
  _spi->begin(_sclk, _miso, _mosi, _cs);
  pinMode(_cs, OUTPUT);
  pinMode(_rst, OUTPUT);
  pinMode(_int, INPUT);
  pinMode(_gpx, INPUT);
  digitalWrite(_cs, HIGH);
  digitalWrite(_rst, LOW);
  delay(50);
  digitalWrite(_rst, HIGH);
  setup_max();
  dummy();
  while(!(read_reg(rUSBIRQ) & bmOSCOKIRQ)) {delay(10);}
  if(debug_serial) {debug_serial->println("OSCOK good");}

  setup_max();
  print_13_31();
}

void max3421e_SPI::end() {
  //set these to high-Z (intput)
  pinMode(_cs, INPUT);      
  pinMode(_rst, INPUT);
  _spi->end();
}

void max3421e_SPI::setup_max() {
  write_reg(rPINCTL, bmGPXB | bmPOSINT | bmFDUPSPI);                                //0b00010110
  write_reg(rMODE, bmHOST | bmDELAYISO | bmDMPULLDN | bmDPPULLDN);                  //0b11100001 
  write_reg(rUSBIEN, 0x00);                                                         //0b00000000
  write_reg(rCPUCTL, bmIE);                                                         //0b00000001
  write_reg(rHIEN, bmCONDETIE);                                                     //0b00100000
}

void max3421e_SPI::dummy() {
  uint8_t* ret = read_reg_multi((13 << 3), 8);
  free(ret);
  ret = read_reg_multi((13 << 3), 8);
  free(ret);
  ret = read_reg_multi((13 << 3), 8);
  free(ret);
}

bool max3421e_SPI::print_13_31() {
  if(!debug_serial) {return false;} //only print if given a Serial to print too
  print_13_20();
  print_21_31();
  return true;
}

bool max3421e_SPI::print_13_20() {
  if(!debug_serial) {return false;} //only print if given a Serial to print too
  uint8_t* ret = read_reg_multi((13 << 3), 8);
  for(int i = 0; i < 8; i++) {
    debug_serial->printf("REG %i: ", i+13);
    printBinaryByte(ret[i], debug_serial);
  }
  free(ret);
  return true;
}

bool max3421e_SPI::print_21_31() {
  if(!debug_serial) {return false;} //only print if given a Serial to print too
  uint8_t* ret = read_reg_multi((21 << 3), 11);
  for(int i = 0; i < 11; i++) {
    debug_serial->printf("REG %i: ", i+21);
    printBinaryByte(ret[i], debug_serial);
  }
  free(ret);
  return true;
}

//shift reg by 3 bits to the left ("reg 1" = 8)
void max3421e_SPI::write_reg(uint8_t reg, uint8_t data) {
  _spi->beginTransaction(*_spi_setting);
  digitalWrite(_cs, LOW);
  uint8_t command_byte = reg | (1<<1);     //bits [7-3] are the reg bits
  _spi->transfer(command_byte);           //send command byte
  _spi->transfer(data);                   //send data
  digitalWrite(_cs, HIGH);
  _spi->endTransaction();
}

//shift reg by 3 bits to the left ("reg 1" = 8)
uint8_t max3421e_SPI::read_reg(uint8_t reg) {
  uint8_t recived = 0;
  _spi->beginTransaction(*_spi_setting);
  digitalWrite(_cs, LOW);
  uint8_t command_byte = reg;  //bits [7-3] are the reg bits
  _spi->transfer(command_byte); //send command byte
  recived = _spi->transfer(0xFF);   //read responce
  digitalWrite(_cs, HIGH);
  _spi->endTransaction();
  return recived;
}

void max3421e_SPI::write_reg_multi(uint8_t reg, uint8_t* data, uint8_t num_bytes) {
  uint8_t* temp = (uint8_t*)heap_caps_malloc(num_bytes, MALLOC_CAP_8BIT);
  memcpy(temp, data, num_bytes);
  _spi->beginTransaction(*_spi_setting);
  digitalWrite(CS, LOW);
  uint8_t command_byte = reg | (1<<1);
  _spi->transfer(command_byte);             //send command byte
  _spi->transfer(temp, num_bytes);          //send data
  digitalWrite(CS, HIGH);
  _spi->endTransaction();
  free(temp);
}

//remember to free return value after completion
uint8_t* max3421e_SPI::read_reg_multi(uint8_t reg, uint8_t num_bytes) {
  uint8_t* temp = (uint8_t*)heap_caps_malloc(num_bytes, MALLOC_CAP_8BIT);
  _spi->beginTransaction(*_spi_setting);
  digitalWrite(_cs, LOW);
  uint8_t command_byte = reg;
  _spi->transfer(command_byte);             //send command byte
  _spi->transfer(temp, num_bytes);          //get data
  digitalWrite(_cs, HIGH);
  _spi->endTransaction();
  return temp;
}

void max3421e_SPI::set_reg_bit(uint8_t reg, uint8_t bit) {
  uint8_t reg_data = read_reg(reg);
  reg_data |= (bit);
  write_reg(reg, reg_data);
}

void max3421e_SPI::clear_reg_bit(uint8_t reg, uint8_t bit) {
  uint8_t reg_data = read_reg(reg);
  reg_data &= ~(bit);
  write_reg(reg, reg_data);
}

void max3421e_SPI::flip_reg_bit(uint8_t reg, uint8_t bit) {
  uint8_t reg_data = read_reg(reg);
  reg_data ^= (bit);
  write_reg(reg, reg_data);
}

uint8_t max3421e_SPI::read_status() {
  uint8_t cmd_rec = 0;
  _spi->beginTransaction(*_spi_setting);
  digitalWrite(_cs, LOW);
  cmd_rec = _spi->transfer(0xFF);
  digitalWrite(_cs, HIGH);
  _spi->endTransaction();
  return cmd_rec;
}

void max3421e_SPI::sample_bus() {
  write_reg(rHCTL, bmSAMPLEBUS);
}

void max3421e_SPI::set_debug_serial(HardwareSerial* serial) {
  debug_serial = serial;
}
