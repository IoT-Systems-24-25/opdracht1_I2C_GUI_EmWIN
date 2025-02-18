#ifndef TC74_H
#define TC74_H

#include <stdint.h>  // Voor uint8_t, int32_t

// Init de driver en stel de I2C-snelheid in
int32_t TC74_DriverInitialize(void);

// Check of het device ACK geeft
int32_t TC74_CheckReady(void);

// Zet de TC74 uit standby (SHDN=0)
int32_t TC74_Init(void);

// Lees 1 byte temperatuur uit register 0x00
int32_t TC74_ReadTemperature(uint8_t *outTemp);

#endif // TC74_H
