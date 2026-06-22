#if !defined(_hid_max3421e_)

#define _hid_max3421e_

#include "usb.h"
#include "usb_descriptors.h"
#include <Arduino.h>
#include <esp_heap_caps.h>

#define bGetReport      0x01
#define bGetIdle        0x02
#define bGetProtocol    0x03
#define bSetReport      0x09
#define bSetIdle        0x0A
#define bSetProtocol    0x0B

class max3421e_HID : public max3421e_usb {
  public:
    max3421e_HID(SPIClass* spi);
    max3421e_HID(int spi_speed, SPIClass* spi);
    max3421e_HID(uint8_t mosi_pin, uint8_t miso_pin, uint8_t sclk_pin, uint8_t cs_pin, uint8_t gpx_pin, uint8_t int_pin, uint8_t rst_pin, SPIClass* spi);
    max3421e_HID(uint8_t mosi_pin, uint8_t miso_pin, uint8_t sclk_pin, uint8_t cs_pin, uint8_t gpx_pin, uint8_t int_pin, uint8_t rst_pin, int spi_speed, SPIClass* spi);
    int USB_HID_GET_REPORT_DESCRIPTOR(Report_Descriptor* rd, uint8_t interface, uint8_t report_type, uint16_t report_length);
    int USB_HID_GET_REPORT(uint8_t interface, uint8_t report_type, uint16_t report_length);
    int USB_HID_GET(int ep);
    void init();
    int put_boot_mode();
  protected:
};



#endif //_hid_max3421e_aa