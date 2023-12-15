#include <stdint.h>
#include <stdbool.h>

#include "../mstimer.h"
#include "../i2c.h"

#include "i2c-this.h"

static int16_t _temperature8bfdp = 0;
char TemperatureIsValid = 0;

int16_t TemperatureConvert8bfdpToTenths(int16_t fixed)
{
    int32_t tenthsFixed = (int32_t)fixed * 10;
    tenthsFixed += 128; // Add 0.05 to round to nearest rather than down
    int16_t tenths = (int16_t)(tenthsFixed >> 8);
    return tenths;
}
int16_t TemperatureConvertTenthsTo8bfdp(int16_t tenths)
{
    int32_t tenthsFixed = (int32_t)tenths << 8;
    int16_t fixed = (int16_t)(tenthsFixed / 10);
    return fixed;
}

int16_t TemperatureGetAs8bfdp(void)
{
    return _temperature8bfdp;
}
int16_t TemperatureGetAsTenths()
{
    return TemperatureConvert8bfdpToTenths(_temperature8bfdp);
}
#define OVERSAMPLE_RATE 5
static void oldaddSample(int16_t value)
{
    static int32_t total = 0;
    static uint16_t count = 0;
    static char    valid = 0;
    
    TemperatureIsValid = 1;
    total += value;
    count++;
    if (count >= (1 << OVERSAMPLE_RATE))
    {
        _temperature8bfdp = (int16_t)(total >> OVERSAMPLE_RATE);
        total = 0;
        count = 0;
        valid = 1;
    }
    else
    {
        if (!valid) _temperature8bfdp = (int16_t)(total / count);
    }
}
#define DIVISOR 4
static void addSample(int16_t value)
{
    /*
     Fastest slew rate is half an hour per degree == 1800 seconds per degree
     Adding 1/256th degree per 500ms == 1/128 degree per second == 128 seconds per degree
     */
    if (!TemperatureIsValid)
    {
        TemperatureIsValid = 1;
        _temperature8bfdp = value;
        return;
    }
    static int8_t _preDivider = 0;
    if (value > _temperature8bfdp) _preDivider++;
    if (value < _temperature8bfdp) _preDivider--;
    if (_preDivider >  DIVISOR) { _temperature8bfdp++; _preDivider = 0; }
    if (_preDivider < -DIVISOR) { _temperature8bfdp--; _preDivider = 0; }
}

void TemperatureMain()
{
    static uint32_t msTimerRepetitive = 0;
    if (MsTimerRepetitive(&msTimerRepetitive, 500UL))
    {
        uint8_t bytes[2]; //Holds a signed 16 bit number
        int     result = 0;

        //Prevent temp register being changed mid read by putting device in shutdown mode
        bytes[0] = 1; //Set pointer to configuration register
        bytes[1] = 1; //Shutdown mode
        I2CSend(I2C_ADDRESS_LM75A, 2, bytes, &result);
        if (result) return;
        
        //Read the temperature
        bytes[0] = 0; //Set pointer to temperature register
        I2CSend(I2C_ADDRESS_LM75A, 1, bytes, &result);
        if (result) return;
        
        bytes[0] = 0x80; //-128 degrees which does not exist so cannot be returned
        bytes[1] = 0x01; //The unused small fraction are not used and are returned as zero
        I2CReceive(I2C_ADDRESS_LM75A, 2, bytes, &result);
        if (bytes[0] == 0x80) return;
        if (bytes[1] == 0x01) return;
        if (result) return;
        uint16_t msb = bytes[0];
        uint16_t lsb = bytes[1];
        uint16_t data16bit = (msb << 8) + (lsb & 0xE0);
        addSample((int16_t)data16bit);
        
        //Start temp updates by putting back to normal mode
        bytes[0] = 1; //Set pointer to configuration register
        bytes[1] = 0; //Normal mode
        I2CSend(I2C_ADDRESS_LM75A, 2, bytes, &result);
        if (result) return;
    }
}