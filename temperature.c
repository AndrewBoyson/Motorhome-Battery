#include <stdint.h>
#include <stdbool.h>

#include "../mstimer.h"
#include "../i2c.h"

#include "i2c-this.h"

static int16_t _temperature8bfdp = 0;
char TemperatureIsValid = 0;
char TemperatureSampleIsReadyForUseByHeater = 0; //Set here by addSample; reset by heater

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
/*
Oversampling; extra bits resolution; speed @ 250ms sampling
0 = 1x      ; 0                    ; 250ms
1 = 2x;     ; 0.5                  ; 500ms
2 = 4x;     ; 1                    ; 1s
3 = 8x;     ; 1.5                  ; 2s
4 = 16x;    ; 2                    ; 4s
5 = 32x;    ; 2.5                  ; 8s
6 = 64x;    ; 3                    ; 16s
7 = 128x;   ; 3.5                  ; 32s
8 = 256x;   ; 4                    ; 64s
 
 LM75A          uses 11 bits to represent +/- 127 deg min 300ms or 3 significant binary places
 ADT7410 should use  16 bits to represent +/- 255 deg min 240ms or 7 significant binary places
 ADT7410 only   uses 13 bits to represent +/- 255 deg min 240ms or 4 significant binary places
 Observed that ADT7410 varied by 4 or bit 2 over several seconds,which equates to only 5 significant binary places. Solution was to add 4 more places by sampling over a minute.
 LM75A   is naturally 8bfdp ie 0bNNNN NNNN FFF0 0000
 ADT7410 is naturally 7bfdp ie 0bNNNN NNNN NFFF FFFF so has to be shifted left by 1 bit to make it 8bfdp
*/
#define SAMPLE_INTERVAL_MS 250
#define BIT_SHIFT_LEFT_TO_MAKE_8BFDP 1
#define OVERSAMPLE_RATE 8
#define   DECIMATE_RATE (OVERSAMPLE_RATE - BIT_SHIFT_LEFT_TO_MAKE_8BFDP)
static void addSample(int16_t value)
{
    //On startup use the first sample
    if (!TemperatureIsValid)
    {
        _temperature8bfdp = value << BIT_SHIFT_LEFT_TO_MAKE_8BFDP;
        TemperatureSampleIsReadyForUseByHeater = 1;
        TemperatureIsValid = 1;
        return;
    }
    
    //After startup oversample and decimate
    static int32_t total = 0;
    static uint16_t count = 0;
    total += value;
    count++;
    if (count >= (1 << OVERSAMPLE_RATE))
    {
        _temperature8bfdp = (int16_t)(total >> DECIMATE_RATE);
        total = 0;
        count = 0;
        TemperatureSampleIsReadyForUseByHeater = 1;
    }
}
static void oldaddSample(int16_t value)
{
    /*
     Value is 7 bit fixed decimal place
     Fastest slew rate is half an hour per degree == 1800 seconds per degree
     Adding 1/256th degree per 500ms == 1/128 degree per second == 128 seconds per degree
     */
    static char hadFirstValue = 0;
    static int16_t firstValue = 0;
    if (!hadFirstValue)
    {
        firstValue = value;
        hadFirstValue = 1;
        return;
    }
    value += firstValue;
    hadFirstValue = 0;
    
    if (!TemperatureIsValid)
    {
        TemperatureIsValid = 1;
        _temperature8bfdp = value;
        return;
    }
    if (value > _temperature8bfdp) _temperature8bfdp++;
    if (value < _temperature8bfdp) _temperature8bfdp--;
}

void TemperatureMain()
{
    static uint32_t msTimerRepetitive = 0;
    if (MsTimerRepetitive(&msTimerRepetitive, SAMPLE_INTERVAL_MS))
    {
        uint8_t bytes[2]; //Holds a signed 16 bit number
        int     result = 0;

        //Assume at this point that the conversion has completed and that bit (6:5) = 11 = shutdown
        //Set pointer to temperature register
        bytes[0] = 0; //Set pointer to temperature register
        I2CSend(I2C_ADDRESS_ADT7410, 1, bytes, &result);
        if (result) return;
        
        //Read the temperature
        bytes[0] = 0x80; //-128 degrees which does not exist so cannot be returned
        I2CReceive(I2C_ADDRESS_ADT7410, 2, bytes, &result);
        if (bytes[0] == 0x80) return;
        if (result) return;
        uint16_t msb = bytes[0];
        uint16_t lsb = bytes[1];
        uint16_t data16bit = (msb << 8) + lsb;
        addSample((int16_t)data16bit);
        
        //Start the next conversion by setting to one shot mode and 16 bit
        bytes[0] = 3; //Set pointer to configuration register
        bytes[1] = 0xA0; //bit 7 = 1 (16bit); bit (6:5) = 01 = one shot. Conversion time is typically 240 ms.
        I2CSend(I2C_ADDRESS_ADT7410, 2, bytes, &result);
        if (result) return;
    }
}

void oldTemperatureMain()
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