/* Author: Damian Amerman-Smith
 * Config file for Macrocontroller's NB firmware.
 * Includes pin configurations for different Macrocontroller hardware.
 * For defined constants (e.g. adding a song to playlist), go to constants.h
 */
#ifndef CONFIG_H
#define CONFIG_H

#include "constants.h"

// Set one of the defines below to 1 to select the hardware configuration
#define BIG_BOARD 0     // Macrocontroller v1
#define NB_MB 1         // Northbridge/Memory Block extended devkit


#if BIG_BOARD       // Macrocontroller v1   
  // Pins for Northbridge-CPU SPI
  #define VSPI_CS 18
  #define VSPI_CLK 5
  #define VSPI_MOSI 17
  #define VSPI_MISO 19
  #define VSPI_D2 21
  #define VSPI_D3 16
  #define D_RDY VSPI_D2
  #define CMD_RDY VSPI_D3

  // Pins for Northbridge-Southbridge UART
  #define UART_TX 33    // old NB/MB: 16, big board: 33
  #define UART_RX 32    // old NB/MB: 17, big board: 32

  // Pins for Memory Block Connections (main board means Damian's NB/MB)
  #define HSPI_CLK 14
  #define HSPI_MOSI 13
  #define HSPI_MISO 12
  #define HSPI_CS_SD 26
  #define HSPI_CS_FL 27
  #define HSPI_CS_PS 15
#elif NB_MB
  // Pins for Northbridge-CPU SPI
  #define VSPI_CS 5     // big board: 5
  #define VSPI_CLK 18     // big board: 18
  #define VSPI_MOSI 23   // (D0) old NB/MB: 23, big board: 17
  #define VSPI_MISO 19   // (D1) big board: 19
  #define VSPI_D2 21     // big board: 21
  #define VSPI_D3 22     // old NB/MB: 22, big board: 16

  // Pins for Northbridge-Southbridge UART
  #define UART_TX 16    // old NB/MB: 16, big board: 33
  #define UART_RX 17    // old NB/MB: 17, big board: 32

  // Pins for Memory Block Connections (main board means Damian's NB/MB)
  #define HSPI_CLK 14
  #define HSPI_MOSI 13
  #define HSPI_MISO 12
  #define HSPI_CS_SD 4  // old NB/MB: 4, big board: 26
  #define HSPI_CS_FL 15   // old NB/MB: 15, big board: 27
  #define HSPI_CS_PS 2   // old NB/MB: 2, big board: 15
#endif





#endif
