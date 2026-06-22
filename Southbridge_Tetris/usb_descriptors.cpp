
#include "usb_descriptors.h"
#include "HID_extras.h"
#include "_helper_fuctions_jobo_.h"
#include <Arduino.h>
#include <esp_heap_caps.h>



/*DESCRIPTOR*/
Descriptor::~Descriptor() {
  for(int i = 0; i < SDL_length; i++) {
    SDL[i]->~Descriptor();
  }
  free(SDL);
}

void Descriptor::parse_list(uint8_t* data, int data_len, HardwareSerial* serial) {
  set_with_byte_stream(data);
  shift_buffer_left(data, &data_len, bLength);
  if(serial) {print(serial);}
  create_SDL(data, data_len, serial);
  if(SDL_length) {
    for(int i = 0; i < SDL_length; i++) {
      SDL[i]->parse_list(data, data_len, serial);
    }
  }
}


//should have been virtual
void Descriptor::set_with_byte_stream(uint8_t* data) {}


/*DEVICE_DESCRIPTOR*/
void Device_Descriptor::set_with_byte_stream(uint8_t* data) {
  bLength = data[0];
  bDescriptorType = data[1];
  bcdUSB = ((data[3] << 8) | data[2]);
  bDeviceClass = data[4];
  bDeviceSubClass = data[5];
  bDeviceProtocol = data[6];
  bMaxPacketSize = data[7];
  idVendor = ((data[9] << 8) | data[8]);
  idProduct = ((data[11] << 8) | data[10]);
  bcdDevice = ((data[13] << 8) | data[12]);
  iManufacturer = data[14];
  iProduct = data[15];
  iSerialNumber = data[16];
  bNumConfigurations = data[17];
}

uint8_t* Device_Descriptor::get_as_byte_stream() {
  struct device_descriptor dd;
  
  dd.bLength = bLength;
  dd.bDescriptorType = bDescriptorType;
  dd.bcdUSB = bcdUSB;
  dd.bDeviceClass = bDeviceClass;
  dd.bDeviceSubClass = bDeviceSubClass;
  dd.bDeviceProtocol = bDeviceProtocol;
  dd.bMaxPacketSize = bMaxPacketSize;
  dd.idVendor = idVendor;
  dd.idProduct = idProduct;
  dd.bcdDevice = bcdDevice;
  dd.iManufacturer = iManufacturer;
  dd.iProduct = iProduct;
  dd.iSerialNumber = iSerialNumber;
  dd.bNumConfigurations = bNumConfigurations;

  return (uint8_t*) &dd;
}

void Device_Descriptor::print(HardwareSerial* serial) {
  if(!serial) {return;}
  serial->println("\tDevice Descriptor");
  serial->printf("bLength: %i\n", bLength);
  serial->printf("bDescriptorType: %i\n", bDescriptorType);
  serial->printf("bcdUSB: %i\n", bcdUSB);
  serial->printf("bDeviceClass: %i\n", bDeviceClass);
  serial->printf("bDeviceSubClass: %i\n", bDeviceSubClass);
  serial->printf("bDeviceProtocol: %i\n", bDeviceProtocol);
  serial->printf("bMaxPacketSize: %i\n", bMaxPacketSize);
  serial->printf("idVendor: %i\n", idVendor);
  serial->printf("idProduct: %i\n", idProduct);
  serial->printf("bcdDevice: %i\n", bcdDevice);
  serial->printf("iManufacturer: %i\n", iManufacturer);
  serial->printf("iProduct: %i\n", iProduct);
  serial->printf("iSerialNumber: %i\n", iSerialNumber);
  serial->printf("bNumConfigurations: %i\n", bNumConfigurations);
}

void Device_Descriptor::create_SDL(uint8_t* data, int data_len, HardwareSerial* serial) {
  SDL_length = bNumConfigurations;
  int size = SDL_length * sizeof(Descriptor*);
  SDL = (Descriptor**)heap_caps_malloc(size, MALLOC_CAP_8BIT);
  for(int i = 0; i < SDL_length; i++) {
    SDL[i] = new Config_Descriptor();
  }
}


/*CONFIGURATION_DESCRIPTOR*/
void Config_Descriptor::set_with_byte_stream(uint8_t* data) {
  bLength = data[0];
  bDescriptorType = data[1];
  wTotalLength = ((data[3] << 8) | data[2]);
  bNumInterfaces = data[4];
  bConfigurationValue = data[5];
  iConfiguration = data[6];
  bmAttributes = data[7];
  bMaxPower = data[8];
}

uint8_t* Config_Descriptor::get_as_byte_stream() {
  struct config_descriptor cd;

  cd.bLength = bLength;
  cd.bDescriptorType = bDescriptorType;
  cd.wTotalLength = wTotalLength;
  cd.bNumInterfaces = bNumInterfaces;
  cd.bConfigurationValue = bConfigurationValue;
  cd.iConfiguration = iConfiguration;
  cd.bmAttributes = bmAttributes;
  cd.bMaxPower = bMaxPower;

  return (uint8_t*) &cd;
}

void Config_Descriptor::print(HardwareSerial* serial) {
  if(!serial) {return;}
  serial->println("\tConfiguration Descriptor");
  serial->printf("bLength: %i\n", bLength);
  serial->printf("bDescriptorType: %i\n", bDescriptorType);
  serial->printf("wTotalLength: %i\n", wTotalLength);
  serial->printf("bNumInterfaces: %i\n", bNumInterfaces);
  serial->printf("bConfigurationValue: %i\n", bConfigurationValue);
  serial->printf("iConfiguration: %i\n", iConfiguration);
  serial->print("bmAttributes: ");
  printBinaryByte(bmAttributes, serial);
  serial->printf("bMaxPower: %i\n", bMaxPower);
}

void Config_Descriptor::create_SDL(uint8_t* data, int data_len, HardwareSerial* serial) {
  SDL_length = bNumInterfaces;
  int size = SDL_length * sizeof(Descriptor*);
  SDL = (Descriptor**)heap_caps_malloc(size, MALLOC_CAP_8BIT);
  for(int i = 0; i < SDL_length; i++) {
    SDL[i] = new Interface_Descriptor();
  }
}


/*INTERFACE_DESCRIPTOR*/
Interface_Descriptor::~Interface_Descriptor() {
  HIDD->~Descriptor();
  Descriptor::~Descriptor();
}

void Interface_Descriptor::set_with_byte_stream(uint8_t* data) {
  bLength = data[0];
  bDescriptorType = data[1];
  bInterfaceNumber = data[2];
  bAlternateSetting = data[3];
  bNumEndpoints = data[4];
  bInterfaceClass = data[5];
  bInterfaceSubClass = data[6];
  bInterfaceProtocol = data[7];
  iInterface = data[8];
}

uint8_t* Interface_Descriptor::get_as_byte_stream() {
  struct interface_descriptor id;

  id.bLength = bLength;
  id.bDescriptorType = bDescriptorType;
  id.bInterfaceNumber = bInterfaceNumber;
  id.bAlternateSetting = bAlternateSetting;
  id.bNumEndpoints = bNumEndpoints;
  id.bInterfaceClass = bInterfaceClass;
  id.bInterfaceSubClass = bInterfaceSubClass;
  id.bInterfaceProtocol = bInterfaceProtocol;
  id.iInterface = iInterface;

  return (uint8_t*) &id;
}

void Interface_Descriptor::print(HardwareSerial* serial) {
  if(!serial) {return;}
  serial->println("\tInterface Descriptor");
  serial->printf("bLength: %i\n", bLength);
  serial->printf("bDescriptorType: %i\n", bDescriptorType);
  serial->printf("bInterfaceNumber: %i\n", bInterfaceNumber);
  serial->printf("bAlternateSetting: %i\n", bAlternateSetting);
  serial->printf("bNumEndpoints: %i\n", bNumEndpoints);
  serial->printf("bInterfaceClass: %i\n", bInterfaceClass);
  serial->printf("bInterfaceSubClass: %i\n", bInterfaceSubClass);
  serial->printf("bInterfaceProtocol: %i\n", bInterfaceProtocol);
  serial->printf("iInterface: %i\n", iInterface);
}

void Interface_Descriptor::create_SDL(uint8_t* data, int data_len, HardwareSerial* serial) {
  if(data[1] == dtHID) {      //check if next descriptor is an HID descriptor
    HIDD = new HID_Descriptor();
    HIDD->parse_list(data, data_len, serial);
  }
  SDL_length = bNumEndpoints;
  int size = SDL_length * sizeof(Descriptor*);
  SDL = (Descriptor**)heap_caps_malloc(size, MALLOC_CAP_8BIT);
  for(int i = 0; i < SDL_length; i++) {
    SDL[i] = new Endpoint_Descriptor();
  }
}


/*ENDPOINT_DESCRIPTOR*/
void Endpoint_Descriptor::set_with_byte_stream(uint8_t* data) {  
  bLength = data[0];
  bDescriptorType = data[1];
  bEndpointAddress = data[2];
  bmAttributes = data[3];
  wMaxPacketSize = ((data[5] << 8) | data[4]);
  bInterval = data[6];
}

uint8_t* Endpoint_Descriptor::get_as_byte_stream() {
  struct endpoint_descriptor ed;
  ed.bLength = bLength;
  ed.bDescriptorType = bDescriptorType;
  ed.bEndpointAddress = bEndpointAddress;
  ed.bmAttributes = bmAttributes;
  ed.wMaxPacketSize = wMaxPacketSize;
  ed.bInterval = bInterval;
  return (uint8_t*) &ed;
}

void Endpoint_Descriptor::print(HardwareSerial* serial) {
  if(!serial) {return;}
  serial->println("\tEndpoint Descriptor");
  serial->printf("bLength: %i\n", bLength);
  serial->printf("bDescriptorType: %i\n", bDescriptorType);
  serial->printf("bEndpointAddress: %i", bEndpointAddress & 0x0F);
  if(bEndpointAddress & (1<<7)) {
    serial->println(" IN");
  } else {
    serial->println(" OUT");
  }
  serial->printf("bmAttributes: ");
  printBinaryByte(bmAttributes, serial);
  serial->printf("wMaxPacketSize: %i\n", wMaxPacketSize);
  serial->printf("bInterval: %i\n", bInterval);
}

void Endpoint_Descriptor::create_SDL(uint8_t* data, int data_len, HardwareSerial* serial) {
  SDL = nullptr;
  SDL_length = 0;
}


/*HID_DESCRIPTOR*/
void HID_Descriptor::set_with_byte_stream(uint8_t* data) {
  bLength = data[0];
  bDescriptorType = data[1];
  bcdHID = ((data[3] << 8) | data[2]);
  bCountryCode = data[4];
  bNumDescriptors = data[5];
  bReportDescriptorType = data[6];
  bReportDescriptorLength = ((data[8] << 8) | data[7]);
}

//TODO allow for aditional Descriptors
uint8_t* HID_Descriptor::get_as_byte_stream() {
  struct HID_descriptor hd;
  
  hd.bLength = bLength;
  hd.bDescriptorType = bDescriptorType;
  hd.bcdHID = bcdHID;
  hd.bCountryCode =bCountryCode;
  hd.bNumDescriptors = bNumDescriptors;
  hd.bReportDescriptorType = bReportDescriptorType;
  hd.bReportDescriptorLength = bReportDescriptorLength;

  return (uint8_t*) &hd;
}

void HID_Descriptor::print(HardwareSerial* serial) {
  if(!serial) {return;}
  serial->println("\tHID Descriptor");
  serial->printf("bLength: %i\n", bLength);
  serial->printf("bDescriptorType: %i\n", bDescriptorType);
  serial->printf("bcdHID: %i\n", bcdHID);
  serial->printf("bCountryCode: %i\n", bCountryCode);
  serial->printf("bNumDescriptors: %i\n", bNumDescriptors);
  serial->printf("bReportDescriptorType: %i\n", bReportDescriptorType);
  serial->printf("bReportDescriptorLength: %i\n", bReportDescriptorLength);
}

void HID_Descriptor::create_SDL(uint8_t* data, int data_len, HardwareSerial* serial) {
  SDL_length = 1;
  int size = SDL_length * sizeof(Descriptor*);
  SDL = (Descriptor**)heap_caps_malloc(size, MALLOC_CAP_8BIT);
  SDL[0] = new Report_Descriptor;
}


/*REPORT_DESCRIPTOR*/
Report_Descriptor::~Report_Descriptor() {
  free(report_data);
}

void Report_Descriptor::create_SDL(uint8_t* data, int data_len, HardwareSerial* serial) {
  SDL = nullptr;
  SDL_length = 0;
}

void Report_Descriptor::set_report_data(uint8_t* data, int data_length) {
  free(report_data);
  report_data = (uint8_t*)heap_caps_malloc(data_length, MALLOC_CAP_8BIT);
  memcpy(report_data, data, data_length);
  report_data_length = data_length;
}

uint8_t* Report_Descriptor::get_as_byte_stream() {
  return report_data;
}

void Report_Descriptor::print(HardwareSerial* serial) {
  if(!serial) {return;}
  if(report_data) {
    serial->printf("\tReport Descriptor\n");
    for(int i = 0; i < report_data_length; i++)  {
      serial->printf("Data Byte %i: %i,\t\tBIN: ", i, report_data[i]);
      printBinaryByte(report_data[i], serial);
    }
  }
}


void Report_Descriptor::parse_report_data(HardwareSerial* serial) {
  //make a copy of data to mutate
  if(!report_data) {return;}    //return if no data
  uint8_t* data_cpy = (uint8_t*)heap_caps_malloc(report_data_length, MALLOC_CAP_8BIT);
  int cpy_len = report_data_length;
  memcpy(data_cpy, report_data, report_data_length);
  //start with a size 0 array
  item_list = (Item**)heap_caps_malloc(0, MALLOC_CAP_8BIT);   
  item_list_length = 0;\
  while(cpy_len > 0) {
    item_list_resize(&item_list, item_list_length, 1);

    if(data_cpy[0] & long_item_check == long_item) {
      item_list[item_list_length] = new Long_Item(data_cpy[0]);
    } else {
      item_list[item_list_length] = new Short_Item(data_cpy[0]);
    }
    int bytes_used = item_list[item_list_length]->add_data(data_cpy);
    shift_buffer_left(data_cpy, &cpy_len, bytes_used);
    item_list_length += 1;
  }
  int indent = 0;
  for(int i = 0; i < item_list_length; i++) {
    item_list[i]->print_report(&indent, serial);
  }
  free(data_cpy);
}

void Report_Descriptor::item_list_resize(Item*** item_list_ptr, int length, int size_change) {
  int new_len = length + size_change;
  Item** temp = (Item**)heap_caps_malloc(new_len * sizeof(Item*), MALLOC_CAP_8BIT);
  for(int i = 0; i < new_len; i++) {
    temp[i] = nullptr;
  }
  int copy_size = new_len;
  if(new_len > length) {
    copy_size = length;
  }
  memcpy(temp, *item_list_ptr, copy_size * sizeof(Item*));
  free(*item_list_ptr);
  *item_list_ptr = temp;
}

//to fufil virtual 
void Report_Descriptor::set_with_byte_stream(uint8_t* data) {}
