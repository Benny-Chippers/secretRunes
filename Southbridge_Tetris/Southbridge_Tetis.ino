
//note endpoints might be a problem
// control endpoint 0 used for setup

#include "HID.h"
#include "Parser.h"
#include "tetris_song.h"
#include <ESP_I2S.h>  
#include <SoftwareSerial.h>
#include <esp_heap_caps.h>


#define I2S_LRC  33
#define I2S_DIN  25
#define I2S_BCLK 32

#define I2S_DATA_BIT_WIDTH 32

#define BYTES_PER_SAMPLE (I2S_DATA_BIT_WIDTH / 8)


#define BUF_SIZE 1600.0


#define UART_RX 26
#define UART_TX 27
#define NB_BAUD 38400


max3421e_HID* max3421e;
// max3421e_HID* other;
int curr_time = 0;
int HID_poll_time = 0;

bool int_triggered_1 = false;
bool int_triggered_2 = false;
uint8_t char_buf_1[1024] = {0};
int char_buf_len_1 = 0;

uint8_t char_buf_2[1024] = {0};
int char_buf_len_2 = 0;

SPIClass* vspi = new SPIClass(VSPI);
SPIClass* hspi = new SPIClass(HSPI);
SoftwareSerial NBSerial(UART_RX, UART_TX);


#define MUT_TIME 5000
SemaphoreHandle_t tetris_mut = NULL;
TaskHandle_t Task1;



I2SClass i2s;
int cursor = 0;
int full_cursor = 0;
uint8_t* buf = NULL;






void swap_usb(max3421e_HID* max) {
  if(max->connected) {
    Serial.println("\tDisconnected");
    max->connected = false;
    max->end();
  } else {
    Serial.println("\tConnected");
    max->connected = true;
    max->init();
    Serial.println("setup usb");
  }
}
    

void interupt_handler(max3421e_HID* max) {
  Serial.println("interupt_handler:interupt triggered");
  byte HIRQ = max->read_reg(rHIRQ);
  Serial.print("interupt_handler:Printing HIRQ byte: ");
  printBinaryByte(HIRQ, &Serial);
  byte clear_bits = 0;
  if(HIRQ & bmCONDETIRQ) {
    swap_usb(max);
    clear_bits |= bmCONDETIRQ;
  }
  max->write_reg(rHIRQ, clear_bits);
  Serial.println("interupt_handler:interupt complete");
}

void ISR1() {
  int_triggered_1 = true;
}

void ISR2() {
  int_triggered_2 = true;
}



void setup() {
  Serial.begin(115200);
  Serial.println("Serial begin");
  NBSerial.begin(NB_BAUD);
  delay(10);
  max3421e = new max3421e_HID(MOSI, MISO, SCLK, CS, GPX, INT, RST, vspi);
  // max3421e->set_debug_serial(&Serial);
  // other->set_debug_serial(&Serial);
  max3421e->begin();
  Serial.println("First Begun");
  // other->begin();
  Serial.println("Second Begun");
  
  attachInterrupt(digitalPinToInterrupt(INT), ISR1, RISING);
  attachInterrupt(digitalPinToInterrupt(35), ISR2, RISING);
  max3421e->sample_bus();
  // other->sample_bus();
  curr_time = millis();
  HID_poll_time = millis();



  
  i2s.setPins(I2S_BCLK, I2S_LRC, I2S_DIN);
  i2s.begin(I2S_MODE_STD, 8000, I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO);

  // while (tetris == NULL) {
  //   tetris = xSemaphoreCreateMutex();
  // }
  xTaskCreatePinnedToCore(songs, "songs", 1000, NULL, 0, &Task1, 0);
  Serial.println("Post thing");

}

void songs(void* thing) {
  Serial.println("Songs");
  delay(50);
  buf == NULL;
  while(buf == NULL) {
    buf = (uint8_t*)heap_caps_malloc(BUF_SIZE+1, MALLOC_CAP_8BIT);
  }
  
  Serial.println("malloc");
  memset(buf, 0, BUF_SIZE+1);



  Serial.println("Begin song");
  while(1) {
    if(cursor < BUF_SIZE) {
      byte input = song[full_cursor];
      buf[cursor] = input;
      cursor += BYTES_PER_SAMPLE;
      full_cursor += 1;
      if(full_cursor >= SONG_SIZE) {
        full_cursor = 0;
      }
    }
    if(cursor >= BUF_SIZE) {
      i2s.write(buf, cursor-(BYTES_PER_SAMPLE));
      cursor = 3;
    }
  }
}

void loop() {
  if(int_triggered_1) {
    int_triggered_1 = false;
    interupt_handler(max3421e);
  }
  // if(int_triggered_2) {
  //   int_triggered_2 = false;
  //   interupt_handler(other);
  // }
  if(max3421e->connected && millis() >= HID_poll_time+10) {
    HID_poll_time = millis();
    int err = max3421e->USB_HID_GET(1);
    if(err == SUCCESS) {
      char_buf_len_1 = max3421e->get_in_buf(char_buf_1);
      if(char_buf_len_1) {
        Serial.println("Got HID Data 1");
        uint32_t key = keyboard_parse(char_buf_1);
        uint8_t* send = (uint8_t*) &key;
        printBinaryWord(key, &Serial);
        for(int i = 0; i < 4; i++) {
          NBSerial.write(send[i]);
        }
        // NBSerial.write(send, 4);
      }
    }
  }
  // if(other->connected && millis() >= HID_poll_time+10) {
  //   HID_poll_time = millis();
  //   int err = max3421e->USB_HID_GET(1);
  //   if(err == SUCCESS) {
  //     char_buf_len_2 = max3421e->get_in_buf(char_buf_2);
  //     if(char_buf_len_2) {
  //       Serial.println("Got HID Data 2");
  //       for(int i = 0; i < char_buf_len_2; i ++) {
  //         Serial.printf("Byte %i: %i\n", i, char_buf_2[i]);
  //       }
  //     }
  //   }
  // }
  // if(millis() >= curr_time + 1000) {
  //   Serial.print("Second Pulse\n");
  //   curr_time = millis();
  // }
}
