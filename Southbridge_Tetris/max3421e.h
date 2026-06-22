/* Copyright (C) 2011 Circuits At Home, LTD. All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

Contact information
-------------------

Circuits At Home, LTD
Web      :  http://www.circuitsathome.com
e-mail   :  support@circuitsathome.com
 */
#if !defined(_max3421e_h_)

#define _max3421e_h_

#include "_helper_fuctions_jobo_.h"
#include <Arduino.h>
#include <SPI.h>
#include <esp_heap_caps.h>

//default pin settings and SPI_SPEED
#define MOSI 23
#define MISO 19
#define SCLK 18
#define CS   5
#define GPX  4
#define INT  34
#define RST  17

#define SPI_SPEED 1000000

// #define SPI_SPEED 1000


/* MAX3421E register/bit names and bitmasks */


/* MAX3421E command byte format: rrrrr0wa where 'r' is register number  */
//
// MAX3421E Registers in HOST mode.
#define rRCVFIFO    0x08    //1<<3
#define rSNDFIFO    0x10    //2<<3
#define rSUDFIFO    0x20    //4<<3
#define rRCVBC      0x30    //6<<3
#define rSNDBC      0x38    //7<<3

#define rUSBIRQ     0x68    //13<<3
/* USBIRQ Bits  */
#define bmVBUSIRQ   0x40    //b6
#define bmNOVBUSIRQ 0x20    //b5
#define bmOSCOKIRQ  0x01    //b0

#define rUSBIEN     0x70    //14<<3
/* USBIEN Bits  */
#define bmVBUSIE    0x40    //b6
#define bmNOVBUSIE  0x20    //b5
#define bmOSCOKIE   0x01    //b0

#define rUSBCTL     0x78    //15<<3
/* USBCTL Bits  */
#define bmCHIPRES   0x20    //b5
#define bmPWRDOWN   0x10    //b4

#define rCPUCTL     0x80    //16<<3
/* CPUCTL Bits  */
#define bmPUSLEWID1 0x80    //b7
#define bmPULSEWID0 0x40    //b6
#define bmIE        0x01    //b0

#define rPINCTL     0x88    //17<<3
/* PINCTL Bits  */
#define bmFDUPSPI   0x10    //b4
#define bmINTLEVEL  0x08    //b3
#define bmPOSINT    0x04    //b2
#define bmGPXB      0x02    //b1
#define bmGPXA      0x01    //b0
// GPX pin selections
#define GPX_OPERATE 0x00
#define GPX_VBDET   0x01
#define GPX_BUSACT  0x02
#define GPX_SOF     0x03

#define rREVISION   0x90    //18<<3

#define rIOPINS1    0xa0    //20<<3

/* IOPINS1 Bits */
#define bmGPOUT0    0x01
#define bmGPOUT1    0x02
#define bmGPOUT2    0x04
#define bmGPOUT3    0x08
#define bmGPIN0     0x10
#define bmGPIN1     0x20
#define bmGPIN2     0x40
#define bmGPIN3     0x80

#define rIOPINS2    0xa8    //21<<3
/* IOPINS2 Bits */
#define bmGPOUT4    0x01
#define bmGPOUT5    0x02
#define bmGPOUT6    0x04
#define bmGPOUT7    0x08
#define bmGPIN4     0x10
#define bmGPIN5     0x20
#define bmGPIN6     0x40
#define bmGPIN7     0x80

#define rGPINIRQ    0xb0    //22<<3
/* GPINIRQ Bits */
#define bmGPINIRQ0 0x01
#define bmGPINIRQ1 0x02
#define bmGPINIRQ2 0x04
#define bmGPINIRQ3 0x08
#define bmGPINIRQ4 0x10
#define bmGPINIRQ5 0x20
#define bmGPINIRQ6 0x40
#define bmGPINIRQ7 0x80

#define rGPINIEN    0xb8    //23<<3
/* GPINIEN Bits */
#define bmGPINIEN0 0x01
#define bmGPINIEN1 0x02
#define bmGPINIEN2 0x04
#define bmGPINIEN3 0x08
#define bmGPINIEN4 0x10
#define bmGPINIEN5 0x20
#define bmGPINIEN6 0x40
#define bmGPINIEN7 0x80

#define rGPINPOL    0xc0    //24<<3
/* GPINPOL Bits */
#define bmGPINPOL0 0x01
#define bmGPINPOL1 0x02
#define bmGPINPOL2 0x04
#define bmGPINPOL3 0x08
#define bmGPINPOL4 0x10
#define bmGPINPOL5 0x20
#define bmGPINPOL6 0x40
#define bmGPINPOL7 0x80

#define rHIRQ       0xc8    //25<<3
/* HIRQ Bits */
#define bmBUSEVENTIRQ   0x01   // indicates BUS Reset Done or BUS Resume
#define bmRWUIRQ        0x02
#define bmRCVDAVIRQ     0x04
#define bmSNDBAVIRQ     0x08
#define bmSUSDNIRQ      0x10
#define bmCONDETIRQ     0x20
#define bmFRAMEIRQ      0x40
#define bmHXFRDNIRQ     0x80

#define rHIEN           0xd0    //26<<3

/* HIEN Bits */
#define bmBUSEVENTIE    0x01
#define bmRWUIE         0x02
#define bmRCVDAVIE      0x04
#define bmSNDBAVIE      0x08
#define bmSUSDNIE       0x10
#define bmCONDETIE      0x20
#define bmFRAMEIE       0x40
#define bmHXFRDNIE      0x80

#define rMODE           0xd8    //27<<3

/* MODE Bits */
#define bmHOST          0x01
#define bmLOWSPEED      0x02
#define bmHUBPRE        0x04
#define bmSOFKAENAB     0x08
#define bmSEPIRQ        0x10
#define bmDELAYISO      0x20
#define bmDMPULLDN      0x40
#define bmDPPULLDN      0x80

#define rPERADDR    0xe0    //28<<3

#define rHCTL       0xe8    //29<<3
/* HCTL Bits */
#define bmBUSRST        0x01
#define bmFRMRST        0x02
#define bmSAMPLEBUS     0x04
#define bmSIGRSM        0x08
#define bmRCVTOG0       0x10
#define bmRCVTOG1       0x20
#define bmSNDTOG0       0x40
#define bmSNDTOG1       0x80

#define rHXFR       0xf0    //30<<3
/* Host transfer token values for writing the HXFR register (R30)   */
/* OR this bit field with the endpoint number in bits 3:0               */
#define tokSETUP  0x10  // HS=0, ISO=0, OUTNIN=0, SETUP=1
#define tokIN     0x00  // HS=0, ISO=0, OUTNIN=0, SETUP=0
#define tokOUT    0x20  // HS=0, ISO=0, OUTNIN=1, SETUP=0
#define tokINHS   0x80  // HS=1, ISO=0, OUTNIN=0, SETUP=0
#define tokOUTHS  0xA0  // HS=1, ISO=0, OUTNIN=1, SETUP=0
#define tokISOIN  0x40  // HS=0, ISO=1, OUTNIN=0, SETUP=0
#define tokISOOUT 0x60  // HS=0, ISO=1, OUTNIN=1, SETUP=0


#define bmSETUP   0x10
#define bmOUTIN   0x20
#define bmISO     0x40
#define bmHS      0x80

#define rHRSL       0xf8    //31<<3

/* HRSL Bits */
#define bmRCVTOGRD  0x10
#define bmSNDTOGRD  0x20
#define bmKSTATUS   0x40
#define bmJSTATUS   0x80
#define bmSE0       0x00    //SE0 - disconnect state
#define bmSE1       0xc0    //SE1 - illegal state


#define MODE_FS_HOST    (bmDPPULLDN|bmDMPULLDN|bmHOST|bmSOFKAENAB)
#define MODE_LS_HOST    (bmDPPULLDN|bmDMPULLDN|bmHOST|bmLOWSPEED|bmSOFKAENAB)


/*USB Transfer type codes*/
#define trSETUP     0
#define trISO       1
#define trBULK      2
#define trHS        3





class max3421e_SPI {
  public:
    max3421e_SPI(SPIClass* spi);
    max3421e_SPI(int spi_speed, SPIClass* spi);
    max3421e_SPI(uint8_t mosi_pin, uint8_t miso_pin, uint8_t sclk_pin, uint8_t cs_pin, uint8_t gpx_pin, uint8_t int_pin, uint8_t rst_pin, SPIClass* spi);
    max3421e_SPI(uint8_t mosi_pin, uint8_t miso_pin, uint8_t sclk_pin, uint8_t cs_pin, uint8_t gpx_pin, uint8_t int_pin, uint8_t rst_pin, int spi_speed, SPIClass* spi);
    void      begin();
    void      end();
    void      setup_max();
    void      write_reg(uint8_t reg, uint8_t data);
    void      write_reg_multi(uint8_t reg, uint8_t* data, uint8_t num_bytes);
    uint8_t   read_reg(uint8_t reg);
    uint8_t*  read_reg_multi(uint8_t reg, uint8_t num_bytes);

    void      set_reg_bit(uint8_t reg, uint8_t bit);
    void      clear_reg_bit(uint8_t reg, uint8_t bit);
    void      flip_reg_bit(uint8_t reg, uint8_t bit);

    bool      print_13_31();
    bool      print_13_20();
    bool      print_21_31();

    uint8_t   read_status();
    void      sample_bus();
    void      set_debug_serial(HardwareSerial* serial);
  protected:
    HardwareSerial* debug_serial = nullptr;
  private:
    uint8_t   _mosi = 0;
    uint8_t   _miso = 0;
    uint8_t   _sclk = 0;
    uint8_t   _cs = 0;
    uint8_t   _gpx = 0;
    uint8_t   _int = 0;
    uint8_t   _rst = 0;
    SPISettings* _spi_setting = nullptr;
    SPIClass* _spi = nullptr;
    void dummy();
};




#endif //_max3421e_h_
