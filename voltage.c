#include <stdint.h>

#include "voltage.h"
#include "adc.h"

#define MV_SUBTRACTED 8199
#define VOLTAGE_DIVISOR_4DP  20364

int16_t VoltageGetAsMv()
{
    if (!AdcValueIsValid) return 0;
    int32_t adcMv = (ADC_VREF_MV * (int32_t)AdcGetValue()) >> ADC_BITS;
    int32_t batteryVoltage = (adcMv * VOLTAGE_DIVISOR_4DP) / 10000 + MV_SUBTRACTED;
    return (int16_t)batteryVoltage;
}