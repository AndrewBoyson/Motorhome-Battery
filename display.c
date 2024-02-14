#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <xc.h>
#include <string.h>
#include <stdlib.h>

#include "../msticker.h"
#include "../mstimer.h"
#include "../eeprom.h"
#include "../lcd-1602.h"

#include "voltage.h"
#include "adc.h"
#include "pulse.h"
#include "count.h"
#include "temperature.h"
#include "keypad.h"
#include "eeprom-this.h"
#include "output.h"
#include "heater.h"

#define REPEAT_TIME_MS    1000

#define PAGE_NONE         0
#define PAGE_HOME         1
#define PAGE_CURRENT      2
#define PAGE_SOC_COUNTED  3
#define PAGE_OUTPUT       4
#define PAGE_HEATER       5
#define MAX_PAGE 5

#define LINE_LENGTH 16

static char line0[17];
static char line1[17];

static uint8_t _displayOnTime  = 0;
static  int8_t _page           = 1; //0 == lcd off
static  int8_t _setting        = 0; //0 == display page

void DisplayInit()
{
    _displayOnTime = EepromReadU8(EEPROM_DISPLAY_ON_TIME_U8);
}

static uint32_t getStayOnTimeSeconds()
{
    switch (_displayOnTime)
    {
        case  1: return 10UL;
        case  2: return 30UL;
        case  3: return  1UL * 60;
        case  4: return 10UL * 60;
        case  5: return 30UL * 60;
        case  6: return  1UL * 60 * 60;
        case  7: return  3UL * 60 * 60;
        case  8: return  6UL * 60 * 60;
        case  9: return 12UL * 60 * 60;
        case 10: return  1UL * 24 * 60 * 60;
        case 11: return  3UL * 24 * 60 * 60;
        case 12: return  1UL * 7 * 24 * 60 * 60;
        case 13: return  2UL * 7 * 24 * 60 * 60;
        case 14: return  4UL * 7 * 24 * 60 * 60;
        default: return 0UL;
    }
}
static void incrementStayOnFactor()
{
    if (_displayOnTime < 14) _displayOnTime++;
    else                     _displayOnTime = 14;
    EepromSaveU8(EEPROM_DISPLAY_ON_TIME_U8, _displayOnTime);
}
static void decrementStayOnFactor()
{
    if (_displayOnTime > 0) _displayOnTime--;
    EepromSaveU8(EEPROM_DISPLAY_ON_TIME_U8, _displayOnTime);
}
static int addStayOnTimeText(char* p) //Returns a length of 10
{
    switch (_displayOnTime)
    {
        case  1: strcpy(p, "10 seconds"); break;
        case  2: strcpy(p, "30 seconds"); break;
        case  3: strcpy(p, "1 minute  "); break;
        case  4: strcpy(p, "10 minutes"); break;
        case  5: strcpy(p, "30 minutes"); break;
        case  6: strcpy(p, "1 hour    "); break;
        case  7: strcpy(p, "3 hours   "); break;
        case  8: strcpy(p, "6 hours   "); break;
        case  9: strcpy(p, "12 hours  "); break;
        case 10: strcpy(p, "1 day     "); break;
        case 11: strcpy(p, "3 days    "); break;
        case 12: strcpy(p, "1 week    "); break;
        case 13: strcpy(p, "2 weeks   "); break;
        case 14: strcpy(p, "4 weeks   "); break;
        default: strcpy(p, "Indefinite"); break;
    }
    return 10;
}
static int addCurrent(char* p, int32_t ma) //Puts current into a string of 6 characters followed by a NUL -> 7 in total
{
    char polarity = ma >= 0;
    if (!polarity) ma = -ma;
    if      (ma == 0 ) p[0] = ' ';
    else if (polarity) p[0] = '+';
    else               p[0] = '-';
    if (ma > 999999L)
    {
        p[1] = '>';
        p[2] = '9';
        p[3] = '9';
        p[4] = '9';
        p[5] = 'A';
        p[6] = 0;
    }
    else
    {
        p++;
        int32_t amps = ma / 1000;
        int32_t rem  = ma % 1000;
        if      (amps > 99) snprintf(p, 6, "%3ldA "      , amps           ); //"120A "
        else if (amps >  9) snprintf(p, 6, "%2ld.%01ldA" , amps, rem / 100); //"99.1A"
        else if (amps >  0) snprintf(p, 6, "%1ld.%02ldA" , amps, rem /  10); //"3.75A"
        else                snprintf(p, 6, "%3ldmA"      , ma             ); //"533mA"
    }
    return 6;
}
static int addVoltage(char* p, int16_t mv) //Puts voltage into a string of 7 characters
{
    snprintf(p, 8, "%2d.%03dV", mv / 1000, mv % 1000);
    return 7;
}
static int addTemperatureTenths(char* p, int16_t tenths) //Puts temperature into a string of 5 characters '-9.9*' to '99.9*'
{
    char positive = tenths >= 0;
    if (positive)
    {
        snprintf(p, 5, "%2d.%d", tenths / 10, tenths % 10);
    }
    else
    {
        tenths = -tenths;
        snprintf(p, 5, "-%1d.%d", tenths / 10, tenths % 10); //NB format is not el dee but one dee
    }
    *(p+4) = 0xdf; //lcd degree symbol
    return 5;
}
static int addPercentSigned(char* p, int8_t percent) //Puts a percentage into a string of 4 characters
{
    snprintf(p, 5, "%3d%%", percent);
    return 4;
}
static int addPercent(char* p, uint8_t percent) //Puts a percentage into a string of 4 characters
{
    snprintf(p, 5, "%3u%%", percent);
    return 4;
}
static int addString(char* p, const char* pText)
{
    int length = 0;
    while(*pText && length < LINE_LENGTH)
    {
        *p++ = *pText++;
        length++;
    }
    return length;
}
static void displayHome0()
{
    int16_t mv = VoltageGetAsMv();
    int32_t ma = PulseGetCurrentMa();
    int16_t tempTenths = TemperatureGetAsTenths();
    char* p = line0;
    p += addString(p, "Home ");
    p += addTemperatureTenths(p, tempTenths);
    p += addPercent(p, CountGetSocPercent());
    *p++ = ' ';
    *p++ = OutputGetState();
    
    p = line1;
    p += addCurrent(p, ma);
    p += addString(p, "   ");
    addVoltage(p, mv);
}
static void displayHome1()
{
    addString(line0, "Display off?");
}
static void displayHome2()
{
    addString(line0, "Display on time?");
    addStayOnTimeText(line1);
}
static void displayHome3()
{
    snprintf(line0, 17, "Tick len? %u", MsTickerGetLength());
    snprintf(line1, 17, "Ext-Int %+ldms", MsTickerGetExtMinusIntMs());
}
static void displayHome4()
{
    snprintf(line0, 17, "Scan time %dms", MsTimerScanTime);
}

static void displayCurrent0()
{
    int32_t ma = PulseGetCurrentMa();
    snprintf(line0, 17, "Current ");
    addCurrent(line0 + 8, ma);
    *(line1+0) = PulsePolarityInst ? '+' : '-';
    snprintf(line1 + 1, 16, "%5lu %5lus", PulseGetMsSinceLastPulse() / 1000, PulseInterval / 1000);
}
static void displaySocCounted0()
{
    snprintf(line0, 17, "SoC counted %3u%%", CountGetSocPercent());
    snprintf(line1, 17, "%8lumAh", CountGetSoCmAh());
}
static void displaySocCounted1()
{
    strncpy(line0, "Adjust SoC?", 16);
    snprintf(line1, 17, "%2u%%", CountGetSocPercent());
}
static void displaySocCounted2()
{
    strncpy(line0, "Aging As/hour?", 16);
    snprintf(line1, 17, "%d", CountGetAgingAsPerHour());
}
static void displayOutput0()
{
    int32_t ma = PulseGetCurrentMa();
    strcpy(line0, "Output  ");
    addCurrent(line0 + 8, ma);
    *(line0+14) = ' ';
    *(line0+15) = OutputGetState();
    
    switch (OutputGetState())
    {
        case 'N': snprintf(line1, 17, "%u%% -> %u%%", CountGetSocPercent(), OutputGetTargetSoc() - 1); break;
        case 'C': snprintf(line1, 17, "%u%% -> %u%%", CountGetSocPercent(), OutputGetTargetSoc() + 1); break;
        default:  snprintf(line1, 17, "%u%% -> %u%%", CountGetSocPercent(), OutputGetTargetSoc()    ); break;
    }
}
static void displayOutput1()
{
    int32_t ma = PulseGetCurrentMa();
    strcpy(line0, "Target? ");
    addCurrent(line0 + 8, ma);
    *(line0+14) = ' ';
    *(line0+15) = OutputGetState();
    
    snprintf(line1, 17, "%u%% -> %u%%", CountGetSocPercent(), OutputGetTargetSoc()); 
}
static void displayOutput2()
{
    int32_t ma = PulseGetCurrentMa();
    strcpy(line0, "State?  ");
    addCurrent(line0 + 8, ma);
    *(line0+14) = ' ';
    *(line0+15) = OutputGetState();
    
    if (OutputGetDischargeEnabled()) strcpy(line1+0, "D+ ");
    else                             strcpy(line1+0, "D- ");
    if (OutputGetChargeEnabled   ()) strcpy(line1+3, "C+ ");
    else                             strcpy(line1+3, "C- ");
}
static void displayHeater0()
{
    char* p = line0;
    p += addString(p, "Temp  ");
    p += addTemperatureTenths(p, TemperatureGetAsTenths());
    *p++ = ' ';
    p += addPercent(p, HeaterGetOutputPercent());
    p = line1;
    p += addString(p, "SP    ");
    p += addTemperatureTenths(p, HeaterGetTargetTenths());
    *p++ = ' ';
    p += addPercentSigned(p, HeaterGetOffsetPercent());
}
static void displayHeater1()
{
    char* p = line0;
    p += addString(p, "Temp? ");
    p += addTemperatureTenths(p, TemperatureGetAsTenths());
    *p++ = ' ';
    p += addPercent(p, HeaterGetOutputPercent());
    p = line1;
    p += addString(p, "SP    ");
    p += addTemperatureTenths(p, HeaterGetTargetTenths());
    *p++ = ' ';
    p += addPercentSigned(p, HeaterGetOffsetPercent());
}
static void displayHeater2()
{
    char* p = line0;
    p += addString(p, "Kp? ");
    snprintf(line1, 17, "%d", HeaterGetKp8bfdp());
}
static void displayHeater3()
{
    char* p = line0;
    p += addString(p, "Ki? ");
    snprintf(line1, 17, "%d", HeaterGetKi8bfdp());
}

static uint32_t addUint32(uint32_t oldValue, uint32_t amount) { return oldValue < (uint32_t)-amount ?           oldValue + amount  : (uint32_t)-1; }
static uint16_t addUint16(uint16_t oldValue, uint16_t amount) { return oldValue < (uint16_t)-amount ?           oldValue + amount  : (uint16_t)-1; }
static uint8_t  addUint8 (uint8_t  oldValue, uint8_t  amount) { return oldValue < (uint8_t )-amount ? (uint8_t)(oldValue + amount) : (uint8_t )-1; }
static uint32_t subUint32(uint32_t oldValue, uint32_t amount) { return oldValue >            amount ?           oldValue - amount  :            0; }
static uint16_t subUint16(uint16_t oldValue, uint16_t amount) { return oldValue >            amount ?           oldValue - amount  :            0; }
static uint8_t  subUint8 (uint8_t  oldValue, uint8_t  amount) { return oldValue >            amount ? (uint8_t)(oldValue - amount) :            0; }

static void adjustSetting(char increase, uint16_t amount)
{
    switch (_page)
    {
        case PAGE_HOME:
            switch (_setting)
            {
                case 1:
                {
                    if (increase) _page = PAGE_NONE; //Turn off display
                    _setting = 0;
                    break;
                }
                case 2:
                {
                    if (increase) incrementStayOnFactor();
                    else          decrementStayOnFactor();
                    break;
                }
                case 3:
                {
                    uint16_t newValue;
                    if (increase) newValue = addUint16(MsTickerGetLength(), amount);
                    else          newValue = subUint16(MsTickerGetLength(), amount);
                    MsTickerSetLength(newValue);
                    break;
                }
            }
            break;
        case PAGE_CURRENT:
            break;
        case PAGE_SOC_COUNTED:
            switch (_setting)
            {
                case 1:
                {
                    if (amount > 10) amount = 10;
                    if (increase) CountAddSocPercent((uint8_t)amount);
                    else          CountSubSocPercent((uint8_t)amount);
                    break;
                }
                case 2:
                {
                    int16_t newValue;
                    if (increase) newValue = CountGetAgingAsPerHour() + (int16_t)amount;
                    else          newValue = CountGetAgingAsPerHour() - (int16_t)amount;
                    CountSetAgingAsPerHour(newValue);
                    break;
                }
            }
            break;
        case PAGE_OUTPUT:
            switch (_setting)
            {
                case 1:
                {
                    if (amount > 10) amount = 10;
                    uint8_t newValue;
                    if (increase) newValue = addUint8(OutputGetTargetSoc(), (uint8_t)amount);
                    else          newValue = subUint8(OutputGetTargetSoc(), (uint8_t)amount);
                    if (newValue > 100) newValue = 100;
                    OutputSetTargetSoc(newValue);
                    break;
                }
                case 2:
                {
                    if (increase) OutputSetChargeEnabled   (!OutputGetChargeEnabled   ());
                    else          OutputSetDischargeEnabled(!OutputGetDischargeEnabled());
                    break;
                }
            }
            break;
        case PAGE_HEATER:
            switch (_setting)
            {
                case 1:
                {
                    if (amount > 10) amount = 10;
                    int16_t newValue;
                    if (increase) newValue = HeaterGetTargetTenths() + (int16_t)amount;
                    else          newValue = HeaterGetTargetTenths() - (int16_t)amount;
                    if (newValue > 300) newValue = 300;
                    if (newValue <   0) newValue =   0;
                    HeaterSetTargetTenths(newValue);
                    break;
                }
                case 2:
                {
                    if (amount > 100) amount = 100;
                    uint16_t newValue;
                    if (increase) newValue = HeaterGetKp8bfdp() + (uint16_t)amount;
                    else          newValue = HeaterGetKp8bfdp() - (uint16_t)amount;
                    HeaterSetKp8bfdp(newValue);
                    break;
                }
                case 3:
                {
                    if (amount > 100) amount = 100;
                    uint16_t newValue;
                    if (increase) newValue = HeaterGetKi8bfdp() + (uint16_t)amount;
                    else          newValue = HeaterGetKi8bfdp() - (uint16_t)amount;
                    HeaterSetKi8bfdp(newValue);
                    break;
                }
            }
            break;
    }
}
void DisplayMain()
{
    static uint32_t msStayOnTimer = 0;
    static uint32_t msRepetitiveTimer = 0;
    static int8_t     currentPage = 0;
    static int8_t     currentSetting = 0;
    
    char settingChanged = 0;
    unsigned int amount = 1;
    switch (KeypadMultiplier)
    {
        case 0:  amount =     1; break;
        case 1:  amount =    10; break;
        case 2:  amount =   100; break;
        default: amount =  1000; break;
    }
    
    if (KeypadOneShot) msStayOnTimer = MsTimerCount;
    uint32_t stayOnTimeMs = getStayOnTimeSeconds() * 1000;
    if (stayOnTimeMs && MsTimerRelative(msStayOnTimer, stayOnTimeMs)) _page = 0;
    
    static int8_t lastPage    = 1;
    static int8_t lastSetting = 0;
    
    if (KeypadOneShot)
    {
        if (!_page)
        {
            _page = lastPage ? lastPage : PAGE_HOME;
            _setting = lastSetting;
        }
        else
        {
            if (KeypadOneShot & 1) //Home
            {
                if (_setting) _setting = 0;
                else          _page = PAGE_HOME;
            }

            if (KeypadOneShot & 2) //Down
            {
                if (_setting)
                {
                    adjustSetting(0, amount);
                }
                else
                {
                    _page--;
                    if (_page < 1) _page = MAX_PAGE;
                }
            }

            if (KeypadOneShot & 4) //Up
            {
                if (_setting)
                {
                    adjustSetting(1, amount);
                }
                else
                {
                    _page++;
                    if (_page > MAX_PAGE) _page = 1;
                }
            }

            if (KeypadOneShot & 8) //Toggle setting and display mode
            {
                if (_setting)
                {
                    _setting++;
                    switch (_page)
                    {
                        case PAGE_HOME        : if (_setting > 4) _setting = 0; break;
                        case PAGE_CURRENT     : if (_setting > 0) _setting = 0; break;
                        case PAGE_SOC_COUNTED : if (_setting > 2) _setting = 0; break;
                        case PAGE_OUTPUT      : if (_setting > 2) _setting = 0; break;
                        case PAGE_HEATER      : if (_setting > 3) _setting = 0; break;
                    }
                }
                else
                {
                    _setting = 1;
                }
            }
            lastPage = _page;
            lastSetting = _setting;
        }
    }
    
    if (!_page &&  LcdIsOn()) LcdTurnOff();
    if ( _page && !LcdIsOn()) LcdTurnOn();
    
    if ( _page && 
         LcdIsReady() && 
         (MsTimerRepetitive(&msRepetitiveTimer, REPEAT_TIME_MS) || _page != currentPage || _setting != currentSetting || settingChanged))
    {
        for (int i = 0; i < 16; i++) line0[i] = ' ';
        for (int i = 0; i < 16; i++) line1[i] = ' ';
        switch (_page)
        {
            case PAGE_HOME:
            {
                switch (_setting)
                {
                    case 0: displayHome0(); break;
                    case 1: displayHome1(); break;
                    case 2: displayHome2(); break;
                    case 3: displayHome3(); break;
                    case 4: displayHome4(); break;
                }
               break;
            }
            case PAGE_CURRENT: displayCurrent0(); break;
            case PAGE_SOC_COUNTED:
            {
                switch (_setting)
                {
                    case 0: displaySocCounted0(); break;
                    case 1: displaySocCounted1(); break;
                    case 2: displaySocCounted2(); break;
                }
                break;
            }
            case PAGE_OUTPUT:
            {
                switch (_setting)
                {
                    case 0: displayOutput0(); break;
                    case 1: displayOutput1(); break;
                    case 2: displayOutput2(); break;
                }
                break;
            }
            case PAGE_HEATER:
            {
                switch (_setting)
                {
                    case 0: displayHeater0(); break;
                    case 1: displayHeater1(); break;
                    case 2: displayHeater2(); break;
                    case 3: displayHeater3(); break;
                }
                break;
            }
        }
       
       LcdSendText(line0, line1);
       currentPage = _page;
       currentSetting = _setting;
    }
}
