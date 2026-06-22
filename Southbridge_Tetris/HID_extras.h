#if !defined(_HID_extras_)

#define _HID_extras_

#include <Arduino.h>

#define type_MAIN   0
#define type_GLOBAL 1
#define type_LOCAL  2

#define long_item           0b11110010  
#define long_item_check     0b11110011    //only need to check tag and size

//main tags -> type_MAIN
//usually data size for all main is 0
#define tag_INPUT           0b1000
#define tag_OUTPUT          0b1001
#define tag_FEATURE         0b1011
#define tag_COLLECTION      0b1010
#define tag_END_COLLECTION  0b1100

//global tags -> type_GLOBAL
#define tag_USAGE           0b0000
#define tag_LOG_MIN         0b0001
#define tag_LOG_MAX         0b0010
#define tag_PHY_MIN         0b0011
#define tag_PHY_MAX         0b0100
#define tag_UNIT_EXP        0b0101
#define tag_UNIT            0b0110
#define tag_REPORT_SIZE     0b0111
#define tag_REPORT_ID       0b1000
#define tag_REPORT_COUNT    0b1001
#define tag_PUSH            0b1010
#define tag_POP             0b1011

//local tags
// #define tag_USAGE           0b0000;    //USAGE is the same for both local and global
#define tag_USAGE_MIN       0b0001
#define tag_USAGE_MAX       0b0010
#define tag_DESIG_INDEX     0b0011
#define tag_DESIG_MIN       0b0100
#define tag_DESIG_MAX       0b0101
#define tag_STR_INDEX       0b0111
#define tag_STR_MIN         0b1000
#define tag_STR_MAX         0b1001
#define tag_DELIMITER       0b1010



class Item {
  public:
    Item(uint8_t first_byte);
    ~Item();
    virtual int add_data(uint8_t* data) = 0;
    virtual void print(HardwareSerial* serial) = 0;
    virtual void print_report(int* indent, HardwareSerial* serial) = 0;

    uint8_t bSize = 0;
    uint8_t bType = 0;
    uint8_t bTag = 0;
    uint8_t* item_data = nullptr;
};

class Short_Item : public Item {
  public:
    Short_Item(uint8_t first_byte);
    int add_data(uint8_t* data);
    void print(HardwareSerial* serial);
    void print_report(int* indent, HardwareSerial* serial);
};

class Long_Item : public Item {
  public:
    Long_Item(uint8_t first_byte);
    int add_data(uint8_t* data);
    void print(HardwareSerial* serial);
    void print_report(int* indent, HardwareSerial* serial);

    uint8_t bDataSize = 0;
    uint8_t bLongItemTag = 0;
};





















#endif //_HID_extras_