# secretRunes
Microcontroller code for the Northbridge, responsible for letting the CPU access memory and peripheral devices.

## Layout

| Path | Purpose |
|------|---------|
| `CPU_SB_sim/` | Program to test NB-SB UART and NB-CPU SPI communication. Intended to run on a ESP32 module connected to the Northbridge/Memory-Block|
| `NB_firmware/` | Up-to-date firmware for NB microcontroller. Implements communication, memory drivers, and multiprogramming to handle SB and CPU transactions simultaneously. |
| `NB_WAV/` | Archaic script for initial System Verification. NB-SB communication bottlenecked music communication, so we added the hardware driver within the NB. This let us play music directly after getting it from the SD card. |
