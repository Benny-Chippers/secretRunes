#include "HID.h"
#include "usb_descriptors.h"
#include <Arduino.h>
#include <esp_heap_caps.h>

max3421e_HID::max3421e_HID(SPIClass* spi) : max3421e_usb(spi) {}
max3421e_HID::max3421e_HID(int spi_speed, SPIClass* spi) : max3421e_usb(spi_speed, spi) {}
max3421e_HID::max3421e_HID(uint8_t mosi_pin, uint8_t miso_pin, uint8_t sclk_pin, uint8_t cs_pin, uint8_t gpx_pin, uint8_t int_pin, uint8_t rst_pin, SPIClass* spi) : 
                            max3421e_usb(mosi_pin, miso_pin, sclk_pin, cs_pin, gpx_pin, int_pin, rst_pin, spi) {}
max3421e_HID::max3421e_HID(uint8_t mosi_pin, uint8_t miso_pin, uint8_t sclk_pin, uint8_t cs_pin, uint8_t gpx_pin, uint8_t int_pin, uint8_t rst_pin, int spi_speed, SPIClass* spi) : 
                            max3421e_usb(mosi_pin, miso_pin, sclk_pin, cs_pin, gpx_pin, int_pin, rst_pin, spi_speed, spi) {}


void max3421e_HID::init() {
  max3421e_usb::init();
  //populate each HID Report Descriptor
  //get all configurations
  Config_Descriptor** config_list = (Config_Descriptor**)dd->SDL;
  int config_len = dd->SDL_length;
  //for each configuration
  for(int i = 0; i < config_len; i++) {
    //get all interfaces
    Interface_Descriptor** interface_list = (Interface_Descriptor**)config_list[i]->SDL;
    int interface_len = config_list[i]->SDL_length;
    //for each interface
    for(int j = 0; j < interface_len; j++) {
      //check if it has an HID descriptor
      if(interface_list[j]->HIDD) {
        HID_Descriptor* HIDD = interface_list[j]->HIDD;
        Report_Descriptor* rd = (Report_Descriptor*)HIDD->SDL[0];    //report descriptor is always index 0
        USB_HID_GET_REPORT_DESCRIPTOR(rd, j,HIDD->bReportDescriptorType , HIDD->bReportDescriptorLength);
      }
    }
  }
}

//type: 1 Input, 2 Output, 3 Feature
int max3421e_HID::USB_HID_GET_REPORT_DESCRIPTOR(Report_Descriptor* rd, uint8_t interface, uint8_t report_type, uint16_t report_length) {
  if(debug_serial) {debug_serial->println("USB_HID_GET_REPORT_DESCRIPTOR:Getting HID report");}
  //TODO fix with updated descriptors
  struct control_request setup_packet;
  setup_packet = create_control_request(0b10000001, bGetDescriptor, 0x00, report_type, interface, report_length);
  USB_CONTROL_TRANSFER(&setup_packet, dd->bMaxPacketSize);
  if(debug_serial) {debug_serial->println("USB_HID_GET_REPORT_DESCRIPTOR:Report in in_buf");}
  rd->set_report_data(in_buf, in_buf_length);
  // rd->print(debug_serial);
  rd->parse_report_data(debug_serial);
  return SUCCESS;
}


int max3421e_HID::USB_HID_GET_REPORT(uint8_t interface, uint8_t report_type, uint16_t report_length) {
  if(debug_serial) {debug_serial->println("USB_HID_GET_REPORT:Getting HID report");}
  //TODO fix with updated descriptors
  struct control_request setup_packet;
  setup_packet = create_control_request(0b10100001, bGetReport, 0x01, report_type, interface, report_length);
  return USB_CONTROL_TRANSFER(&setup_packet, dd->bMaxPacketSize);
}


int max3421e_HID::USB_HID_GET(int ep) {
  if(debug_serial) {debug_serial->println("USB_HID_GET:clearing buffer");}
  clear_in_buffer();
  uint8_t control_byte = ep & 0x0F;
  int err;
  do{
    err = send_control(control_byte);
    if(err) {return err;}
    err = get_appened_rcv_data();
    if(err) {return err;}
  } while (err != SUCCESS);  
  return SUCCESS;
}


int max3421e_HID::put_boot_mode() {
  //set interface 0 to boot mode
  struct control_request setup_packet;
  if(debug_serial) {debug_serial->println("Setting Boot mode");}
  setup_packet = create_control_request(0b00100001, bSetProtocol, 0x0000, 0x00, 0);
  USB_CONTROL_TRANSFER(&setup_packet, 64);
  if(debug_serial) {debug_serial->println("Getting idle");}
  setup_packet = create_control_request(0b10100001, bGetIdle, 0x01, 0x00, 0x00, 1);
  USB_CONTROL_TRANSFER(&setup_packet, 64);
  if(in_buf_length) {
    for(int i = 0; i < in_buf_length; i++) {
      if(debug_serial) {debug_serial->printf("Byte %i: %i\n", i, in_buf[i]);}
    }
  }
  USB_TRANSFER(0, trBULK, 1, 64);
  if(in_buf_length) {
    for(int i = 0; i < in_buf_length; i++) {
      if(debug_serial) {debug_serial->printf("Byte %i: %i\n", i, in_buf[i]);}
    }
  }
  while(1);

}

// set idles
// set report 
