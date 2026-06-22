
#include "HID_extras.h"
#include "_helper_fuctions_jobo_.h"
#include <Arduino.h>

Item::Item(uint8_t first_byte) {
  bSize = first_byte & 0b00000011;
  bType = (first_byte & 0b00001100) >> 2;
  bTag = (first_byte & 0xF0) >> 4;
}

Item::~Item() {
  free(item_data);
}

//use Item constructor
Short_Item::Short_Item(uint8_t first_byte) : Item(first_byte) {}

int Short_Item::add_data(uint8_t* data) {
  int len = bSize;
  if(len == 3) {
    len = 4;
  }
  item_data = (uint8_t*)heap_caps_malloc(len, MALLOC_CAP_8BIT); 
  for(int i = 0; i < len; i++) {
    item_data[i] = data[i+1];
  }
  return len+1;   //return how many bytes where used
}
void Short_Item::print(HardwareSerial* serial) {
  if(!serial) {return;}   //if we dont have a serial to print too, dont print
  serial->println("\tShort Item");
  serial->printf("Size: ");
  if(bSize == 3) {
    serial->println("4");
  } else {
    serial->printf("%i\n", bSize);
  }
  serial->printf("Type: %i: ", bType);
  switch (bType) {
    case type_MAIN:
      serial->println("Main");
      serial->printf("Tag: %i: ", bTag);
      switch (bTag) {
        case tag_INPUT:
          serial->println("Input");
          break;
        case tag_OUTPUT:
          serial->println("Output");
          break;
        case tag_FEATURE:
          serial->println("Feature");
          break;
        case tag_COLLECTION:
          serial->println("Collection");
          break;
        case tag_END_COLLECTION:
          serial->println("End Collection");
          break;
        default:
          serial->println("Reserved");
          break;
      }
      break;
    case type_GLOBAL:
      serial->println("Global");
      serial->printf("Tag: %i: ", bTag);
      switch (bTag) {
        case tag_USAGE:
          serial->println("Usage page");
          break;
        case tag_LOG_MIN:
          serial->println("Logical Min");
          break;
        case tag_LOG_MAX:
          serial->println("Logical Max");
          break;
        case tag_PHY_MIN:
          serial->println("Physical Min");
          break;
        case tag_PHY_MAX:
          serial->println("Physical Max");
          break;
        case tag_UNIT_EXP:
          serial->println("Unit EXP");
          break;
        case tag_UNIT:
          serial->println("Unit");
          break;
        case tag_REPORT_SIZE:
          serial->println("Report Size");
          break;
        case tag_REPORT_ID:
          serial->println("Report ID");
          break;
        case tag_REPORT_COUNT:
          serial->println("Report Count");
          break;
        case tag_PUSH:
          serial->println("PUSH");
          break;
        case tag_POP:
          serial->println("POP");
          break;
        default:
          serial->println("Resrved");
          break;
      }
      break;
    case type_LOCAL:
      serial->println("Local");
      serial->printf("Tag: %i: ", bTag);
      switch (bTag) {
        case tag_USAGE:
          serial->println("Usage ID");
          break;
        case tag_USAGE_MAX:
          serial->println("Usage Max");
          break;
        case tag_USAGE_MIN:
          serial->println("Usage Min");
          break;
        case tag_DESIG_INDEX:
          serial->println("Designator Index");
          break;
        case tag_DESIG_MIN:
          serial->println("Designator Min");
          break;
        case tag_DESIG_MAX:
          serial->println("Designator Max");
          break;
        case tag_STR_INDEX:
          serial->println("String Index");
          break;
        case tag_STR_MIN:
          serial->println("String Min");
          break;
        case tag_STR_MAX:
          serial->println("String Max");
          break;
        case tag_DELIMITER:
          serial->println("Delimiter");
          break;
        default:
          serial->println("Resrved");
          break;
      }
      break;
    default:
      serial->println("Reserved");
      break;
  }
  int len = bSize;
  if(len == 3) {
    len = 4;
  }
  for(int i = 0; i < len; i++) {
    serial->printf("Data Byte %i: %i\tBIN: ", i, item_data[i]);
    printBinaryByte(item_data[i], serial);
  }
}
void Short_Item::print_report(int* indent, HardwareSerial* serial) {
  if(!serial) {return;}   //if we dont have a serial to print too, dont print
  if(bType == type_MAIN && bTag == tag_END_COLLECTION) {
    *indent -= 1;  
    for(int i = 0; i < *indent; i++) {
      serial->print("\t");
    }
    serial->print("Main End Collection");
  } else {
    for(int i = 0; i < *indent; i++) {
      serial->print("\t");
    }
    switch (bType) {
      case type_MAIN:
        serial->print("Main ");
        switch (bTag) {
          case tag_INPUT:
            serial->print("Input");
            break;
          case tag_OUTPUT:
            serial->print("Output");
            break;
          case tag_FEATURE:
            serial->print("Feature");
            break;
          case tag_COLLECTION:
            serial->print("Collection ");
            *indent += 1;
            break;
          default:
            serial->print("Resrved");
            break;
        }
        break;
      case type_GLOBAL:
        serial->print("Global ");
        switch (bTag) {
          case tag_USAGE:
            serial->print("Usage Page");
            break;
          case tag_LOG_MIN:
            serial->print("Logical Min");
            break;
          case tag_LOG_MAX:
            serial->print("Logical Max");
            break;
          case tag_PHY_MIN:
            serial->print("Physical Min");
            break;
          case tag_PHY_MAX:
            serial->print("Physical Max");
            break;
          case tag_UNIT_EXP:
            serial->print("Unit EXP");
            break;
          case tag_UNIT:
            serial->print("Unit");
            break;
          case tag_REPORT_SIZE:
            serial->print("Report Size");
            break;
          case tag_REPORT_ID:
            serial->print("Report ID");
            break;
          case tag_REPORT_COUNT:
            serial->print("Report Count");
            break;
          case tag_PUSH:
            serial->print("PUSH");
            break;
          case tag_POP:
            serial->print("POP");
            break;
          default:
            serial->print("Resrved");
            break;
        }
        break;
      case type_LOCAL:
        serial->print("Local ");
        switch (bTag) {
          case tag_USAGE:
            serial->print("Usage ID");
            break;
          case tag_USAGE_MAX:
            serial->print("Usage Max");
            break;
          case tag_USAGE_MIN:
            serial->print("Usage Min");
            break;
          case tag_DESIG_INDEX:
            serial->print("Designator Index");
            break;
          case tag_DESIG_MIN:
            serial->print("Designator Min");
            break;
          case tag_DESIG_MAX:
            serial->print("Designator Max");
            break;
          case tag_STR_INDEX:
            serial->print("String Index");
            break;
          case tag_STR_MIN:
            serial->print("String Min");
            break;
          case tag_STR_MAX:
            serial->print("String Max");
            break;
          case tag_DELIMITER:
            serial->print("Delimiter");
            break;
          default:
            serial->print("Resrved");
            break;
        }
        break;
      default:
        serial->print("Reserved ");
        break;
    }
  }
  
  int len = bSize;
  if(len == 3) {
    len = 4;
  }
  for(int i = 0; i < len; i++) {
    serial->printf(", (0d%i,0x%X)", item_data[i], item_data[i]);
  }
  serial->println();
}


//use Item constructor
Long_Item::Long_Item(uint8_t first_byte) : Item(first_byte) {}

int Long_Item::add_data(uint8_t* data) {
  bDataSize = data[1];
  bLongItemTag = data[2];
  item_data = (uint8_t*)heap_caps_malloc(bDataSize, MALLOC_CAP_8BIT); 
  for(int i = 0; i < bDataSize; i++) {
    item_data[i] = data[i+3];
  }
  return bDataSize+3;   //return how many bytes where used
}
void Long_Item::print(HardwareSerial* serial) {
  if(!serial) {return;}   //if we dont have a serial to print too, dont print
  serial->println("\tLong Item");
  serial->printf("bType: %i\n", bType);
  serial->printf("bDataSize: %i\n", bDataSize);
  serial->printf("bLongItemTag: %i\n", bLongItemTag);
  for(int i = 0; i < bDataSize; i++) {
    serial->printf("Data Byte %i: %i\tBIN: ", i, item_data[i]);
    printBinaryByte(item_data[i], serial);
  }
}
void Long_Item::print_report(int* indent, HardwareSerial* serial) {
  if(!serial) {return;}   //if we dont have a serial to print too, dont print
  for(int i = 0; i < *indent; i++) {
    serial->print("\t");
  }
  
  serial->printf("type: %i", bType);
  serial->printf(", Size: %i", bDataSize);
  serial->printf(", Tag: %i", bLongItemTag);
    serial->printf(", Data Bytes");
  for(int i = 0; i < bDataSize; i++) {
    serial->printf(", %i", item_data[i]);
  }
}




