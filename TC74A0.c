#include "TC74A0.h"
#include "Driver_I2C.h"
#include "Driver_Common.h"
#include <string.h>
#include <stdio.h>

// De I2C-driver die je al hebt
extern ARM_DRIVER_I2C Driver_I2C3;
static ARM_DRIVER_I2C *ptrI2C = &Driver_I2C3;

// 7-bit adres van de TC74A0 is 0x48
#define TC74_7BIT_ADDR  (0x48)
#define REG_CONFIG      0x01
#define REG_TEMP        0x00

int32_t TC74_DriverInitialize(void)
{
    int32_t status = ptrI2C->Initialize(NULL);
    if (status != ARM_DRIVER_OK) return -1;

    status = ptrI2C->PowerControl(ARM_POWER_FULL);
    if (status != ARM_DRIVER_OK) return -2;

    status = ptrI2C->Control(ARM_I2C_BUS_SPEED_STANDARD, 0);
    if (status != ARM_DRIVER_OK) return -3;

    return 0; // OK
}

int32_t TC74_CheckReady(void)
{
    uint8_t dummy[1] = {0x00};
    ptrI2C->MasterTransmit(TC74_7BIT_ADDR, dummy, 1, false);
    while (ptrI2C->GetStatus().busy);
    return (ptrI2C->GetDataCount() == 1) ? 0 : -1;
}

int32_t TC74_Init(void)
{
    uint8_t tx[2];
    tx[0] = REG_CONFIG;  // 0x01
    tx[1] = 0x00;        // Normal mode

    ptrI2C->MasterTransmit(TC74_7BIT_ADDR, tx, 2, false);
    while (ptrI2C->GetStatus().busy);

    return (ptrI2C->GetDataCount() == 2) ? 0 : -1;
}

int32_t TC74_ReadTemperature(uint8_t *outTemp)
{
    uint8_t reg = REG_TEMP; // 0x00

    // Schrijf registeradres met repeatedStart
    ptrI2C->MasterTransmit(TC74_7BIT_ADDR, &reg, 1, true);
    while (ptrI2C->GetStatus().busy);
    if (ptrI2C->GetDataCount() != 1) {
        return -1;
    }

    // Lees 1 byte
    ptrI2C->MasterReceive(TC74_7BIT_ADDR, outTemp, 1, false);
    while (ptrI2C->GetStatus().busy);

    return (ptrI2C->GetDataCount() == 1) ? 0 : -2;
}
