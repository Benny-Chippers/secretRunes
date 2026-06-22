


// max3421e_usb::


#include "usb.h"
#include "max3421e.h"
#include "_helper_fuctions_jobo_.h"
#include <Arduino.h>
#include <esp_heap_caps.h>


max3421e_usb::max3421e_usb(SPIClass* spi) : max3421e_SPI(spi) {
  max3421e_usb_setup();
}
max3421e_usb::max3421e_usb(int spi_speed, SPIClass* spi) : max3421e_SPI(spi_speed, spi) {
  max3421e_usb_setup();
}
max3421e_usb::max3421e_usb(uint8_t mosi_pin, uint8_t miso_pin, uint8_t sclk_pin, uint8_t cs_pin, uint8_t gpx_pin, uint8_t int_pin, uint8_t rst_pin, SPIClass* spi) : 
                            max3421e_SPI(mosi_pin, miso_pin, sclk_pin, cs_pin, gpx_pin, int_pin, rst_pin, spi) {
  max3421e_usb_setup();
}
max3421e_usb::max3421e_usb(uint8_t mosi_pin, uint8_t miso_pin, uint8_t sclk_pin, uint8_t cs_pin, uint8_t gpx_pin, uint8_t int_pin, uint8_t rst_pin, int spi_speed, SPIClass* spi) : 
                            max3421e_SPI(mosi_pin, miso_pin, sclk_pin, cs_pin, gpx_pin, int_pin, rst_pin, spi_speed, spi) {
  max3421e_usb_setup();
}

max3421e_usb::~max3421e_usb() {
  free(in_buf);
  free(out_buf);
  free(in_endpoint_pid);
  free(out_endpoint_pid);
}

void max3421e_usb::max3421e_usb_setup() {
  int_triggered = false;
  connected = false;

  in_buf = (uint8_t*)heap_caps_malloc(MAX_IN_BUF_SIZE, MALLOC_CAP_8BIT);
  in_buf_length = 0;
  in_endpoint_pid = (bool*)heap_caps_malloc(16, MALLOC_CAP_8BIT);
  out_buf = (uint8_t*)heap_caps_malloc(MAX_OUT_BUF_SIZE, MALLOC_CAP_8BIT);
  out_buf_length = 0;
  out_endpoint_pid = (bool*)heap_caps_malloc(16, MALLOC_CAP_8BIT);
  last_used_endpoint = 0;

  dd = new Device_Descriptor();
  per_addr = 0;
  
  memset(in_buf, 0, MAX_IN_BUF_SIZE);
  memset(in_endpoint_pid, 0, 16);
  memset(out_buf, 0, MAX_OUT_BUF_SIZE);
  memset(out_endpoint_pid, 0, 16);
}


//to have a data phase, make sure wLength > 1
//for a control write, configure out_buf and out_buf_size before calling this function
//like with USB_TRASNFER, max write size is 64 bytes
int max3421e_usb::USB_CONTROL_TRANSFER(struct control_request *setup_packet, int max_byte_transfer) {
  int err = 0;
  if(debug_serial) {debug_serial->println("USB_CONTROL_TRASFER:Sending Control Transfer");}
  write_reg_multi(rSUDFIFO, (uint8_t*)setup_packet, 8);
  err = send_control(0b00010000);
  if(err) {return err;}
  if(debug_serial) {debug_serial->println("USB_CONTROL_TRASFER:Setup Phase Complete");}
  int dir = !(setup_packet->bmRequestType & (1<<7));
  if(setup_packet->wLength) {
    if(dir) {//need to do this for some reason
      write_reg(rHCTL, bmSNDTOG1);
    } else {
      write_reg(rHCTL, bmRCVTOG1);    
    }
    err = USB_TRANSFER(dir, trBULK, 0, max_byte_transfer);
    if(err) {return err;}
    if(debug_serial) {debug_serial->println("USB_CONTROL_TRASFER:Data Phase Complete");}
  }
  err = USB_TRANSFER(!dir, trHS, 0, max_byte_transfer);
  if(err) {return err;}
  if(debug_serial) {debug_serial->println("USB_CONTROL_TRASFER:HS Phase Complete");}
  return SUCCESS;
}

//ep is the 4 bit endpoint
//dir = 1 means out/write transfer
//1 ISO, 2 BULK, 3 HS, rest are reserved
//for any OUT transfer, make sure out_buf and out_buf_lenght is up to date before calling
//max_byte_per_transfer should not be over 64 Bytes
int max3421e_usb::USB_TRANSFER(uint8_t dir, uint8_t type, uint8_t ep, int max_byte_per_transfer) {
  if(max_byte_per_transfer > 64) {max_byte_per_transfer = 64;}  //cap max_byte_per_transfer at 64
  int err = SUCCESS;
  ep &= 0x0F;

  if(ep != last_used_endpoint) {    //if we are a different endpoint, save and restore toggle
    if(debug_serial) {debug_serial->println("USB_TRANSFER:New Endpoint, Changing PID");}
    if(debug_serial) {debug_serial->printf("USB_TRANSFER:Last endpoint: %i, New endpoint %i\n", last_used_endpoint, ep);}
    save_pid(last_used_endpoint);
    restore_pid(ep);
    last_used_endpoint = ep;
  }

  uint8_t control_byte = ep;
  if(dir) {control_byte |= (1 << 5);}
  switch (type) {   //handshank should never get here
    case trISO:   //ISO
      control_byte |= (1 << 6);
      break;
    case trBULK:   //BULK
      break;  //intentionaly left blank
    case trHS:  //HS
      control_byte |= (1 << 7);
      return send_control(control_byte);
    default:
      return ERR_INVALID_INPUT;
  }
  if(dir) {
    err = OUT_USB_TRANSFER(control_byte, max_byte_per_transfer);
  } else {
    err = IN_USB_TRANSFER(control_byte);
  }
  return err;
}

int max3421e_usb::IN_USB_TRANSFER(uint8_t control_byte) {
  if(debug_serial) {debug_serial->println("IN_USB_TRANSFER:clearing buffer");}
  clear_in_buffer();
  int prev_len = 0;
  int err = SUCCESS;
  do{
    prev_len = in_buf_length;
    err = send_control(control_byte);
    if(err) {return err;}
    err = get_appened_rcv_data();
    if(err) {return err;}
  } while (prev_len != in_buf_length);              //repeat until all recived bytes are int 
  return SUCCESS;
}

int max3421e_usb::OUT_USB_TRANSFER(uint8_t control_byte, int max_byte_per_transfer) {
  int out_buf_sent = 0;   //keep track of how many bytes of out buf has been sent
  int err = SUCCESS;
  while(out_buf_length) {
    if(debug_serial) {debug_serial->println("USB_TRANSFER:Writing out buffer to max3421e");}
    while(!(read_status() & bmSNDBAVIRQ)) {}
    if(out_buf_length > max_byte_per_transfer) {  //&out_buf[out_buf_sent] used so that we always start where the last transfer started
      write_reg_multi(rSNDFIFO, &out_buf[out_buf_sent], max_byte_per_transfer); 
      write_reg(rSNDBC, max_byte_per_transfer);  
    } else {
      write_reg_multi(rSNDFIFO, &out_buf[out_buf_sent], out_buf_length);
      write_reg(rSNDBC, out_buf_length);
    }
    err = send_control(control_byte);
    out_buf_length -= max_byte_per_transfer;
    out_buf_sent += max_byte_per_transfer;
  }
  out_buf_length = 0;
  return SUCCESS;
}

void max3421e_usb::USB_RESET() {
  if(debug_serial) {debug_serial->println("USB Reset");}
  write_reg(rHCTL, bmBUSRST);                 //send bus reset command
  while(read_reg(rHCTL) & bmBUSRST);          //wait for command to complete
  write_reg(rHIRQ, bmFRAMEIRQ);               //clear frame irq
  set_reg_bit(rMODE, bmSOFKAENAB);            //set SOFKAENAB
  while(!(read_reg(rHIRQ) & bmFRAMEIRQ));     //wait for a frame irq
  delay(20);
}


void max3421e_usb::end() {
  clear_in_buffer();
  clear_out_buffer();
  memset(out_endpoint_pid, 0, 16);
  memset(in_endpoint_pid, 0, 16);
  dd->~Device_Descriptor();
  clear_reg_bit(rMODE, bmSOFKAENAB);
}

void max3421e_usb::init() {
  bool j_status_bit = 0;
  while(!j_status_bit) {
    write_reg(rHCTL, bmSAMPLEBUS);
    byte HRSL = read_reg(rHRSL);
    if((HRSL & bmJSTATUS) && !(HRSL & bmKSTATUS)) {
      if(debug_serial) {debug_serial->println("idle j state, keep speed");}
    }
    if((HRSL & bmKSTATUS) && !(HRSL & bmJSTATUS)) {   //idle detected 
      if(debug_serial) {debug_serial->println("idle k state, swap speeds");}
      flip_reg_bit(rMODE, bmLOWSPEED);
    }
    set_reg_bit(rMODE, bmSOFKAENAB);    //set SOFKAENAB

    write_reg(rHCTL, bmSAMPLEBUS);
    j_status_bit = read_reg(rHRSL) & bmJSTATUS;
  }
  send_usb_setup();
}

void max3421e_usb::send_usb_setup() {

  struct control_request setup_packet;

  write_reg(rPERADDR, 0);       //defaul address of peripherals is 0
  USB_RESET();


  clear_in_buffer();
  if(debug_serial) {debug_serial->println("send_usb_setup:Getting device descriptor");}
  //8 byte, GET DESCRIPTOR (8th byte holds max packet size for EP0)
  setup_packet = create_control_request(0b10000000, bGetDescriptor, 0x00, 0x01, 0, 8);
  print_err(USB_CONTROL_TRANSFER(&setup_packet, 8));
  int max_packet_size = in_buf[7];
  if(debug_serial) {debug_serial->printf("send_usb_setup:EP 0 Max Packet Size: %i\n", max_packet_size);}
  clear_in_buffer();

  USB_RESET();
  //set address
  per_addr = (per_addr + 1) % 128;
  if(!per_addr) {per_addr += 1;} //dont assign a peripheral address of 0
  if(debug_serial) {
    debug_serial->print("send_usb_setup:setting address: ");
    debug_serial->println(per_addr);
  }
  setup_packet = create_control_request(0, bSetAddress, per_addr, 0, 0);
  print_err(USB_CONTROL_TRANSFER(&setup_packet, max_packet_size));
  write_reg(rPERADDR, per_addr);
  if(debug_serial) {debug_serial->printf("send_usb_setup:rPERADDR after set: %i\n", read_reg(rPERADDR));}
  //get device descriptor
  setup_packet = create_control_request(0b10000000, bGetDescriptor, 0x00, 0x01, 0, 18);
  print_err(USB_CONTROL_TRANSFER(&setup_packet, max_packet_size));
  
  dd->set_with_byte_stream(in_buf);
  dd->print(debug_serial);

  dd->create_SDL(nullptr, 0, debug_serial);   //only interface descriptor uses create_SDL's arguments
  int num_of_config = dd->SDL_length;
  //for each configuration, get and parse said configuration
  for(int i = 0; i < num_of_config; i++) {
    dd->SDL[i] = new Config_Descriptor;
    get_and_parse_config_descriptor(i, dd->bMaxPacketSize, (Config_Descriptor*)dd->SDL[i]);
  }
  clear_in_buffer();
  //set configuration
  setup_packet = create_control_request(0b00000000, bSetConfig, 1, 0, 0);
  print_err(USB_CONTROL_TRANSFER(&setup_packet, dd->bMaxPacketSize));
  dd->_current_config = 1;
  if(debug_serial) {debug_serial->printf("send_usb_setup:current config: %i\n", dd->_current_config);}
  clear_in_buffer();

}


void max3421e_usb::get_and_parse_config_descriptor(uint8_t index, int max_packet_size, Config_Descriptor* cd) {
  if(debug_serial) {debug_serial->printf("get_and_parse_config_descriptor:Getting config values for config: %i", index);}
  struct control_request setup_packet;
  //get size of configuration
  setup_packet = create_control_request(0b10000000, bGetDescriptor, index, 0x02, 0, 9);
  print_err(USB_CONTROL_TRANSFER(&setup_packet, max_packet_size)); 
  setup_packet.wLength = in_buf[2] | (in_buf[3] << 8); 
  clear_in_buffer();
  //get data for all other discriptors under this configuration
  print_err(USB_CONTROL_TRANSFER(&setup_packet, max_packet_size));
  //create and populate an array for all interfaces 
  cd->parse_list(in_buf, in_buf_length, debug_serial);
}


void max3421e_usb::print_result_code() {
  if(!debug_serial) {return;}
  int result_code = get_result_code();
  switch (result_code) {
    case hrSUCCESS:
      debug_serial->println("hrSUCCESS: Successful Transfer");
      break;
    case hrBUSY:
      debug_serial->println("hrBUSY: Successful Transfer");
      break;
    case hrBADREQ:
      debug_serial->println("hrBADREQ: Bad value in HXFR reg");
      break;
    case hrUNDEF:
      debug_serial->println("hrUNDEF: (reserved)");
      break;
    case hrNAK:
      debug_serial->println("hrNAK: Peripheral returned NAK");
      break;
    case hrSTALL:
      debug_serial->println("hrSTALL: Perpheral returned STALL");
      break;
    case hrTOGERR:
      debug_serial->println("hrTOGERR: Toggle error/ISO over-underrun");
      break;
    case hrWRONGPID:
      debug_serial->println("hrWRONGPID: Received the wrong PID");
      break;
    case hrBADBC:
      debug_serial->println("hrBADBC: Bad byte count");
      break;
    case hrPIDERR:
      debug_serial->println("hrPIDERR: Receive PID is corrupted");
      break;
    case hrPKTERR:
      debug_serial->println("hrPKTERR: Packet error (stuff, EOP)");
      break;
    case hrCRCERR:
      debug_serial->println("hrCRCERR: CRC error");
      break;
    case hrKERR:
      debug_serial->println("hrKERR: K-state instead of response");
      break;
    case hrJERR:
      debug_serial->println("hrJERR: J-state instead of response");
      break;
    case hrTIMEOUT:
      debug_serial->println("hrTIMEOUT: Device did not respond in time");
      break;
    case hrBABBLE:
      debug_serial->println("hrBABBLE: Device talked too long");
      break;
  }
}

uint8_t max3421e_usb::get_result_code() {
  return read_reg(rHRSL) & 0x0F; //we only care about the bottom 4 bits
}

void max3421e_usb::save_pid(uint8_t ep) {
  ep &= 0x0F;   //make sure its max value is 15
  uint8_t grd_byte = read_reg(rHRSL);
  out_endpoint_pid[ep] = grd_byte & bmSNDTOGRD;
  in_endpoint_pid[ep] = grd_byte & bmRCVTOGRD;
    if(debug_serial) {debug_serial->printf("save_pid:out PID: %i, in PID: %i\n", out_endpoint_pid[ep], in_endpoint_pid[ep]);}
}

void max3421e_usb::restore_pid(uint8_t ep) {
  uint8_t command_byte = 0;
  command_byte |= (bmSNDTOG0 << !!(out_endpoint_pid[ep]));
  command_byte |= (bmRCVTOG0 << !!(in_endpoint_pid[ep]));
  if(debug_serial) {debug_serial->printf("restore_pid:out PID: %i, in PID: %i\n", out_endpoint_pid[ep], in_endpoint_pid[ep]);}
  if(debug_serial) {debug_serial->printf("restore_pid:Command Byte: ");}
  if(debug_serial) {printBinaryByte(command_byte, debug_serial);}
  write_reg(rHCTL, command_byte);
}

//continualy write a control byte to rHXFR until a usb SUCCESS is returned
int max3421e_usb::send_control(uint8_t control_byte) {
  write_reg(rHIRQ, bmHXFRDNIRQ);              //make sure IRQ is cleared
  int naks = 0;
  int timeout = 0;
  int j_state = 0;
  int result = 0;
  do {
    write_reg(rHXFR, control_byte);
    while(!(read_reg(rHIRQ) & bmHXFRDNIRQ));    //wait for transfer to be done
    write_reg(rHIRQ, bmHXFRDNIRQ);              //clear IRQ bit
    print_result_code();
    result = get_result_code();
    if(result == hrNAK) {
      naks += 1;
      if(naks >= NAK_LIMIT) {
        if(debug_serial) {debug_serial->println("send_control:NAK limit reached");}
        return ERR_NAK_LIMIT_REACHED;
      }
    }
    if(result == hrTIMEOUT) {
      timeout += 1;
      if(timeout >= TIMEOUT_LIMIT) {
        if(debug_serial) {debug_serial->println("send_control:Timeout limit reached");}
        return ERR_TIMEOUT_LIMIT_REACHED;
      }
    }
    if(result == hrJERR) {
      err_swap_GRD(true);   //if j err, might mean that our out pid is wrong
    }
    if(result == hrWRONGPID) {
      err_swap_GRD(false); //if wrong pid, means out in pid is wrong, flip it
    }
  } while (result != hrSUCCESS);
  return SUCCESS;
}


int max3421e_usb::get_appened_rcv_data() {
  if(!(read_reg(rHIRQ) & bmRCVDAVIRQ)) {      //check if there is data to read
    if(debug_serial) {debug_serial->println("get_appened_rcv_data:No Recived Data");}
    return ERR_NO_RCV_DATA;                                   //return if not
  }
  uint8_t num_bytes = read_reg(rRCVBC);                   //how many bytes to read
  // Serial.printf("number of bytes read: %i\n", num_bytes);
  if(in_buf_length + num_bytes > MAX_IN_BUF_SIZE) {
    if(debug_serial) {debug_serial->println("get_appened_rcv_data:Read would cause buffer overflow");}
    return ERR_BUFFER_OVERFLOW;
  }
  uint8_t* temp = read_reg_multi(rRCVFIFO, num_bytes);    //read that many bytes
  for(int i = in_buf_length; i < in_buf_length + num_bytes; i++) {    //appened them to in_buffer
    in_buf[i] = temp[i];            
  }
  in_buf_length += num_bytes;
  free(temp);
  write_reg(rHIRQ, bmRCVDAVIRQ);
  return SUCCESS;
}

void max3421e_usb::clear_in_buffer() {
    clear_buf(&in_buf, MAX_IN_BUF_SIZE);
    in_buf_length = 0;
}

void max3421e_usb::clear_out_buffer() {
    clear_buf(&out_buf, MAX_OUT_BUF_SIZE);
    out_buf_length = 0;
}

//requires a buffer of 1024 which it will replace
int max3421e_usb::get_in_buf(uint8_t* buf) {
  for(int i = 0; i < in_buf_length; i++) {
    buf[i] = in_buf[i];
  }
  return in_buf_length;
}

//replaces length number of entries in buf
//max length 1024
int max3421e_usb::get_in_buf(uint8_t* buf, int length) {
  for(int i = 0; i < length; i++) {
    buf[i] = in_buf[i];
  }
  return length;
}


//appends all 1024 bytes from in_buf to buf
int max3421e_usb::append_in_buf(uint8_t* buf, int buf_length) {
  for(int i = 0; i < in_buf_length; i++) {
    buf[i + buf_length] = in_buf[i];
  }
  return in_buf_length;
}
//appends all copy_length bytes from in_buf to buf
int max3421e_usb::append_in_buf(uint8_t* buf, int buf_length, int copy_length) {
  for(int i = 0; i < copy_length; i++) {
    buf[i + buf_length] = in_buf[i];
  }
  return copy_length;
}

// dir = 1 means OUT
int max3421e_usb::debug_swap_pid(uint8_t dir, uint8_t ep) {
  ep = ep & 0x0f;
  if(dir) {
    out_endpoint_pid[ep] = !out_endpoint_pid[ep];
    return out_endpoint_pid[ep];
  }
  in_endpoint_pid[ep] = !in_endpoint_pid[ep];
  return in_endpoint_pid[ep];
}


void max3421e_usb::err_swap_GRD(bool SND) {
  if(debug_serial) {debug_serial->println("err_swap_GRD:swapping PID");}
  uint8_t HRSL = read_reg(rHRSL);
  if(SND) {
    bool SNDTOGRD = ((HRSL & bmSNDTOGRD) > 0);
    if(SNDTOGRD) {    //if currently 1
      write_reg(rHCTL, bmSNDTOG0);  //make it 0
    } else {
      write_reg(rHCTL, bmSNDTOG1);  //else make it 1
    }
  } else {
    bool RCVTOGRD = ((HRSL & bmRCVTOGRD) > 0);
    if(RCVTOGRD) {    //if currently 1
      write_reg(rHCTL, bmRCVTOG0);  //make it 0
    } else {
      write_reg(rHCTL, bmRCVTOG1);  //else make it 1
    }
  }
}

void max3421e_usb::print_err(int err) {
  if(!debug_serial) {return;}
  switch (err) {
    case ERR_BUFFER_OVERFLOW:
      debug_serial->println("Error Buffer Overflow");
      break;
    case ERR_INVALID_INPUT:
      debug_serial->println("Error Invalid Input");
      break;
    case ERR_NO_RCV_DATA:
      debug_serial->println("Error no rcv data");
      break;
    case ERR_NOT_HID_INTERFACE:
      debug_serial->println("Error not an HID interface");
      break;
    case ERR_NAK_LIMIT_REACHED:
      debug_serial->println("Error nak limit reached");
      break;
    case ERR_J_STATE:
      debug_serial->println("Error constant J state");
      break;
    case ERR_TIMEOUT_LIMIT_REACHED:
      debug_serial->println("Error timeout limit reached");
      break;
    case SUCCESS:
      debug_serial->println("Success");
      break;                                                                                                                                                                                                                                                                                                                                                                                                                                          
  }
}

struct control_request create_control_request(uint8_t bmRequestType, uint8_t bRequest, uint8_t wValueLo, uint8_t wValueHi, uint16_t wIndex, uint16_t wLength) {
  struct control_request cr;
  cr.bmRequestType = bmRequestType;
  cr.bRequest = bRequest;
  cr.wValue = (wValueHi << 8) | wValueLo;
  cr.wIndex = wIndex;
  cr.wLength = wLength;
  return cr;
}

struct control_request create_control_request(uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, uint16_t wLength) {
  struct control_request cr;
  cr.bmRequestType = bmRequestType;
  cr.bRequest = bRequest;
  cr.wValue = wValue;
  cr.wIndex = wIndex;
  cr.wLength = wLength;
  return cr;
}








