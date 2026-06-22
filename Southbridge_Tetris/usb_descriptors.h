#if !defined(_usb_descriptors_)

#define _usb_descriptors_

#include "HID_extras.h"
#include <Arduino.h>

//Descriptor types
#define dtDEVICE                      1  
#define dtCONFIGURATION               2 
#define dtSTRING                      3  
#define dtINTERFACE                   4  
#define dtENDPOINT                    5  
#define dtDEVICE_QUALIFIRER           6  
#define dtOTHER_SPEED_CONFIGURATION   7  
#define dtINTERFACT_POWER             8  
#define dtHID                         33
#define dtHID_REPORT                  34    




class Descriptor {
  public:
    ~Descriptor();
    Descriptor** SDL = nullptr;   //"sub descriptor list" aka all descriptors lower on the tree
    int SDL_length = 0;

    uint8_t bLength = 0;
    uint8_t bDescriptorType = 0;

    void parse_list(uint8_t* data, int data_len, HardwareSerial* serial);
    virtual void create_SDL(uint8_t* data, int data_len, HardwareSerial* serial) = 0;
    virtual void set_with_byte_stream(uint8_t* data) = 0;
    virtual uint8_t* get_as_byte_stream() = 0;
    virtual void print(HardwareSerial* serial) = 0;
};

class Report_Descriptor : public Descriptor {
  public:
    ~Report_Descriptor();
    bool populated = false;
    uint8_t* report_data = nullptr;
    int report_data_length = 0;
    Item** item_list = nullptr;
    int item_list_length = 0;

    void create_SDL(uint8_t* data, int data_len, HardwareSerial* serial);
    void set_report_data(uint8_t* data, int data_length);
    void parse_report_data(HardwareSerial* serial);
    uint8_t* get_as_byte_stream();
    void print(HardwareSerial* serial);
  private:
    void set_with_byte_stream(uint8_t* data);
    void item_list_resize(Item*** item_list_ptr, int length, int size_change);
};

//size 9 bytes
struct HID_descriptor {
  uint8_t bLength;
  uint8_t bDescriptorType;
  uint16_t bcdHID;
  uint8_t bCountryCode;
  uint8_t bNumDescriptors;
  uint8_t bReportDescriptorType;
  uint16_t bReportDescriptorLength;
};

class HID_Descriptor : public Descriptor {
  public:
    uint16_t bcdHID = 0;
    uint8_t bCountryCode = 0;
    uint8_t bNumDescriptors = 0;
    uint8_t bReportDescriptorType = 0;
    uint16_t bReportDescriptorLength = 0;

    void create_SDL(uint8_t* data, int data_len, HardwareSerial* serial);
    void set_with_byte_stream(uint8_t* data);
    uint8_t* get_as_byte_stream();
    void print(HardwareSerial* serial);
};

//size 7 bytes
struct endpoint_descriptor {
  uint8_t bLength;
  uint8_t bDescriptorType;
  uint8_t bEndpointAddress;
  uint8_t bmAttributes;
  uint16_t wMaxPacketSize;
  uint8_t bInterval;
};

class Endpoint_Descriptor : public Descriptor {
  public:
    uint8_t bEndpointAddress = 0;
    uint8_t bmAttributes = 0;
    uint16_t wMaxPacketSize = 0;
    uint8_t bInterval = 0;
    
    void create_SDL(uint8_t* data, int data_len, HardwareSerial* serial);
    void set_with_byte_stream(uint8_t* data);
    uint8_t* get_as_byte_stream();
    void print(HardwareSerial* serial);
};


//size 9 bytes
struct interface_descriptor {
  uint8_t bLength;
  uint8_t bDescriptorType;
  uint8_t bInterfaceNumber;
  uint8_t bAlternateSetting;
  uint8_t bNumEndpoints;
  uint8_t bInterfaceClass;
  uint8_t bInterfaceSubClass;
  uint8_t bInterfaceProtocol;
  uint8_t iInterface;
};

class Interface_Descriptor : public Descriptor {
  public:
    ~Interface_Descriptor();
    uint8_t bInterfaceNumber = 0;
    uint8_t bAlternateSetting = 0;
    uint8_t bNumEndpoints = 0;
    uint8_t bInterfaceClass = 0;
    uint8_t bInterfaceSubClass = 0;
    uint8_t bInterfaceProtocol = 0;
    uint8_t iInterface = 0;

    HID_Descriptor* HIDD = nullptr;

    void create_SDL(uint8_t* data, int data_len, HardwareSerial* serial);
    void set_with_byte_stream(uint8_t* data);
    uint8_t* get_as_byte_stream();
    void print(HardwareSerial* serial);
};


//size 9 bytes
struct config_descriptor {
  uint8_t bLength;
  uint8_t bDescriptorType;
  uint16_t wTotalLength;
  uint8_t bNumInterfaces;
  uint8_t bConfigurationValue;
  uint8_t iConfiguration;
  uint8_t bmAttributes;
  uint8_t bMaxPower;
};

class Config_Descriptor : public Descriptor {
  public:
    uint16_t wTotalLength = 0;
    uint8_t bNumInterfaces = 0;
    uint8_t bConfigurationValue = 0;
    uint8_t iConfiguration = 0;
    uint8_t bmAttributes = 0;
    uint8_t bMaxPower = 0;

    void create_SDL(uint8_t* data, int data_len, HardwareSerial* serial);
    void set_with_byte_stream(uint8_t* data);
    uint8_t* get_as_byte_stream();
    void print(HardwareSerial* serial);
};


//size 18 bytes
struct device_descriptor {
  uint8_t bLength;
  uint8_t bDescriptorType;
  uint16_t bcdUSB;
  uint8_t bDeviceClass;
  uint8_t bDeviceSubClass;
  uint8_t bDeviceProtocol;
  uint8_t bMaxPacketSize;
  uint16_t idVendor;
  uint16_t idProduct;
  uint16_t bcdDevice;
  uint8_t iManufacturer;
  uint8_t iProduct;
  uint8_t iSerialNumber;
  uint8_t bNumConfigurations;
};

class Device_Descriptor : public Descriptor {
  public:
    uint16_t bcdUSB = 0;
    uint8_t bDeviceClass = 0;
    uint8_t bDeviceSubClass = 0;
    uint8_t bDeviceProtocol = 0;
    uint8_t bMaxPacketSize = 0;
    uint16_t idVendor = 0;
    uint16_t idProduct = 0;
    uint16_t bcdDevice = 0;
    uint8_t iManufacturer = 0;
    uint8_t iProduct = 0;
    uint8_t iSerialNumber = 0;
    uint8_t bNumConfigurations = 0;

    int _current_config = 0;

    void create_SDL(uint8_t* data, int data_len, HardwareSerial* serial);
    void set_with_byte_stream(uint8_t* data);
    uint8_t* get_as_byte_stream();
    void print(HardwareSerial* serial);
};

#endif //_usb_descriptors_