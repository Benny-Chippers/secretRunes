
#if !defined(_usb_max3421e_)

#define _usb_max3421e_


#include "max3421e.h"
#include "usb_descriptors.h"
#include <Arduino.h>
#include <esp_heap_caps.h>

#define MAX_IN_BUF_SIZE         1024
#define MAX_OUT_BUF_SIZE        1024
#define NAK_LIMIT               30
#define TIMEOUT_LIMIT           30

#define ERR_BUFFER_OVERFLOW     -1
#define ERR_INVALID_INPUT       -2
#define ERR_NO_RCV_DATA         -3
#define ERR_NOT_HID_INTERFACE   -4
#define ERR_NAK_LIMIT_REACHED   -5
#define ERR_J_STATE             -6
#define ERR_TIMEOUT_LIMIT_REACHED   -7
#define SUCCESS                 0

/* Host error result codes, the 4 LSB's in the HRSL register */
#define hrSUCCESS   0x00
#define hrBUSY      0x01
#define hrBADREQ    0x02
#define hrUNDEF     0x03
#define hrNAK       0x04
#define hrSTALL     0x05
#define hrTOGERR    0x06
#define hrWRONGPID  0x07
#define hrBADBC     0x08
#define hrPIDERR    0x09
#define hrPKTERR    0x0A
#define hrCRCERR    0x0B
#define hrKERR      0x0C
#define hrJERR      0x0D
#define hrTIMEOUT   0x0E
#define hrBABBLE    0x0F

/*Standard Device Request codes*/
#define bGetStatus      0x00
#define bClearFeature   0x01
#define bSetFeature     0x03
#define bSetAddress     0x05
#define bGetDescriptor  0x06
#define bSetDescriptor  0x07
#define bGetConfig      0x08
#define bSetConfig      0x09




class max3421e_usb : public max3421e_SPI{
  public:
    max3421e_usb(SPIClass* spi);
    max3421e_usb(int spi_speed, SPIClass* spi);
    max3421e_usb(uint8_t mosi_pin, uint8_t miso_pin, uint8_t sclk_pin, uint8_t cs_pin, uint8_t gpx_pin, uint8_t int_pin, uint8_t rst_pin, SPIClass* spi);
    max3421e_usb(uint8_t mosi_pin, uint8_t miso_pin, uint8_t sclk_pin, uint8_t cs_pin, uint8_t gpx_pin, uint8_t int_pin, uint8_t rst_pin, int spi_speed, SPIClass* spi);
    ~max3421e_usb();
    void init();
    void end();
    int USB_CONTROL_TRANSFER(struct control_request *setup_packet, int max_byte_transfer);
    int USB_TRANSFER(uint8_t dir, uint8_t type, uint8_t ep, int max_byte_transfer);
    void USB_RESET();
    
    void add_to_out_buf(uint8_t* buf, int size);
    int get_in_buf(uint8_t* buf);
    int get_in_buf(uint8_t* buf, int length);
    int append_in_buf(uint8_t* buf, int buf_length);
    int append_in_buf(uint8_t* buf, int buf_length, int copy_length);
    int debug_swap_pid(uint8_t dir, uint8_t ep);
    bool connected;
  protected:
    void print_err(int err);
    void err_swap_GRD(bool SND);
    void max3421e_usb_setup();
    
    void send_usb_setup();
    uint8_t get_result_code();
    void print_result_code();

    int send_control(uint8_t control_byte);
    int IN_USB_TRANSFER(uint8_t control_byte);
    int OUT_USB_TRANSFER(uint8_t control_byte, int max_byte_transfer);

    void clear_in_buffer();
    void clear_out_buffer();

    int get_appened_rcv_data();

    void save_pid(uint8_t ep);
    void restore_pid(uint8_t ep);

    void get_and_parse_config_descriptor(uint8_t index, int max_packet_size, Config_Descriptor* cd);

    bool int_triggered;

    //TODO change endpoint pids to uint16_t bitmaps
    uint8_t* in_buf;
    int in_buf_length;
    bool* in_endpoint_pid;
    uint8_t* out_buf;
    int out_buf_length;
    bool* out_endpoint_pid;
    int last_used_endpoint;

    Device_Descriptor* dd;
    uint8_t per_addr;
    // struct interface_descriptor *current_id;
};

struct control_request create_control_request(uint8_t bmRequestType, uint8_t bRequest, uint8_t wValueLo, uint8_t wValueHi, uint16_t wIndex, uint16_t wLength);

struct control_request create_control_request(uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, uint16_t wLength);

struct control_request {
  uint8_t bmRequestType;
  uint8_t bRequest;
  uint16_t wValue;
  uint16_t wIndex;
  uint16_t wLength;
};





#endif //_usb_max3421e_