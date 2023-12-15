#include <stdint.h>

#include "../mstimer.h"
#include "../eeprom.h"

#include "eeprom-this.h"
#include "count.h"

#define BATTERY_CAPACITY_AH 280

static uint32_t _capacityAmpSeconds        = 0;
static uint32_t _ampSeconds                = 0;
static uint8_t  _lastSavedSoc0to255        = 0; //Used to save count across resets. Updated in CountMain
static  int16_t _agingAsPerHour            = 0;

void CountInit()
{
    _lastSavedSoc0to255        = EepromReadU8(EEPROM_COUNT_SOC_U8);
    _capacityAmpSeconds = BATTERY_CAPACITY_AH * 3600UL;
    _ampSeconds = (uint32_t)_lastSavedSoc0to255  * _capacityAmpSeconds / 256;
    _agingAsPerHour = EepromReadS16(EEPROM_COUNT_AGING_AS_PER_HOUR_S16);
}

int16_t CountGetAgingAsPerHour    () { return _agingAsPerHour;}
void    CountSetAgingAsPerHour    ( int16_t v) {_agingAsPerHour = v; EepromSaveS16(EEPROM_COUNT_AGING_AS_PER_HOUR_S16, _agingAsPerHour); } 

uint32_t CountGetAmpSeconds()
{
    return _ampSeconds;
}
void     CountSetAmpSeconds(uint32_t v)
{
    _ampSeconds = v;
}
void     CountAddAmpSecondsU(uint32_t v)
{
    if (_ampSeconds < (_capacityAmpSeconds - v)) _ampSeconds += v;
    else                                         _ampSeconds  = _capacityAmpSeconds;
}
void     CountSubAmpSecondsU(uint32_t v)
{
    if (_ampSeconds > v) _ampSeconds -= v;
    else                 _ampSeconds  = 0;
}
void     CountAddAmpSeconds(int32_t v)
{
    int32_t ampSecondsSigned = (int32_t)_ampSeconds + v; //280 * 3600 = 1,008,000 (000F 6180) so plenty of room
    if (ampSecondsSigned <                            0) ampSecondsSigned = 0;
    if (ampSecondsSigned > (int32_t)_capacityAmpSeconds) ampSecondsSigned = (int32_t)_capacityAmpSeconds;
    _ampSeconds = (uint32_t)ampSecondsSigned;
}

uint16_t CountGetAmpHours()             { return (uint16_t)(CountGetAmpSeconds() / 3600); }
void     CountSetAmpHours(uint16_t v)   {                   CountSetAmpSeconds(v * 3600); } //1AH = 3600 AS = 3600 Coulombs
/*
  0% = -0.5 to   0.4999%
  1% =  0.5 to   1.4999%
 99% = 98.5 to  99.4999%
100% = 99.5 to 100.4999%

so add 0.5 and take whole part
 */
uint8_t  CountGetSocPercent()           { return (uint8_t)(CountGetAmpSeconds()           *  201 / 2 / _capacityAmpSeconds); } // * 201 / 2 was * 100
void     CountSetSocPercent(uint8_t v)  {                  CountSetAmpSeconds((uint32_t)v * _capacityAmpSeconds /  100); }
void     CountAddSocPercent(uint8_t v)
{
    uint32_t toAdd = (uint32_t)v * _capacityAmpSeconds /  100;
    CountAddAmpSecondsU(toAdd);
}
void     CountSubSocPercent(uint8_t v)
{
    uint32_t toAdd = (uint32_t)v * _capacityAmpSeconds /  100;
    CountSubAmpSecondsU(toAdd);
}

uint8_t  CountGetSoc0to255()          { return (uint8_t)(CountGetAmpSeconds()           *  513 / 2 / _capacityAmpSeconds); } //*513 / 2 was * 256
void     CountSetSoc0to255(uint8_t v) {                  CountSetAmpSeconds((uint32_t)v * _capacityAmpSeconds /  256); }

uint32_t CountGetSoCmAh()             { return CountGetAmpSeconds() * 10 / 36; }
void     CountSetSoCmAh(uint32_t v)   {        CountSetAmpSeconds(v * 36 / 10);}


void CountMain()
{
    //Add aging
    static uint32_t _msTimerAging = 0;
    if (MsTimerRepetitive(&_msTimerAging, 60UL*60*1000)) CountAddAmpSeconds(_agingAsPerHour);
    
    //Calculate Soc (0-255) and save if necessary
    uint8_t thisSoc0to255 = CountGetSoc0to255();
    if (thisSoc0to255 != _lastSavedSoc0to255)
    {
        _lastSavedSoc0to255 = thisSoc0to255;
        EepromSaveU8(EEPROM_COUNT_SOC_U8, _lastSavedSoc0to255); //This limits the number of writes to eeprom while still being good enough
    }
}