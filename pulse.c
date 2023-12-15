#include <xc.h>
#include <stdint.h>

#include "../mstimer.h"
#include "../eeprom.h"

#include "eeprom-this.h"
#include "voltage.h"
#include "keypad.h"
#include "count.h"

#define POL  PORTAbits.RA0


/*
 HertzPerVolt = 32.55
 ShuntVolts = 0.075
 ShuntAmps = 50 or 100 or 150
 VoltsPerAmp = ShuntVolts / ShuntAmps
 HertzPerAmp = HertzPerVolt x VoltsPerAmp
 AmpSecondsPerPulse = 1 / HertzPerAmp
 = 1 / (HertzPerVolt x VoltsPerAmp)
 = 1 / (HertzPerVolt x (ShuntVolts / ShuntAmps))
 = ShuntAmps / (HertzPerVolt x ShuntVolts)
    =  50 / (32.55 x 0.075) = 20.481
 or = 150 / (32.55 x 0.075) = 61.444
 
 50mV is max output; hertz per volt is 32.55
 So, at 50mV, time period is 1 / (32.55 * 0.05) = 614mS
 
 Pulse interval at 150A is 61.444 / 150 = 409mS
 Pulse interval at 100mA is 61.444 / 0.1 = 614S
 
 */
#define MA_SECONDS_PER_PULSE 61444
#define MINIMUM_PULSE_INTERVAL_MS 400UL

uint32_t PulseInterval     = 0;
uint32_t PulseMsCount      = 0;
char     PulsePolarity     = 1;
char     PulsePolarityInst = 1;

static uint32_t _maSecondsPerPulse       = 0; //Needs to be 32 bit in case > 65535
static uint32_t _wholeAmpSecsPerPulse    = 0;
static uint16_t _remainingMaSecsPerPulse = 0;

uint32_t PulseGetAbsoluteCurrentMa()
{
    if (!PulseMsCount) return 0; //Current is unknown so zero is as good a value as any
    uint32_t pulseInterval = MsTimerCount - PulseMsCount;
    if (pulseInterval < PulseInterval) pulseInterval = PulseInterval; //Use the longest interval (lowest current);
    
    uint32_t ma;
    if (pulseInterval < 20) ma = 999999; //For short intervals set current to an arbitrarily large number
    else                    ma = (uint32_t)_maSecondsPerPulse * 1000 / pulseInterval;
    return ma;
}
int32_t PulseGetCurrentMa()
{
    uint32_t ma = PulseGetAbsoluteCurrentMa();
    if (PulsePolarity) return  (int32_t)ma;
    else               return -(int32_t)ma;
}

void PulseInit()
{
    INTEDG0 = 0; //Interrupt on falling edge
    INT0IF = 0;
    _maSecondsPerPulse       = MA_SECONDS_PER_PULSE;
    _wholeAmpSecsPerPulse    = _maSecondsPerPulse / 1000;
    _remainingMaSecsPerPulse = (uint16_t)(_maSecondsPerPulse % 1000);
}
uint32_t PulseGetMsSinceLastPulse()
{
    return MsTimerCount - PulseMsCount;
}

void PulseMain()
{
    static char    _isFirst = 1;
    static int16_t _fraction = 0;
    
    PulsePolarityInst = POL;
        
    if (INT0IF)
    {
        INT0IF = 0;
        if (_isFirst)
        {
            _isFirst = 0;
            return;
        }
        uint32_t count = MsTimerCount;
        uint32_t interval = count - PulseMsCount;
        if (interval < MINIMUM_PULSE_INTERVAL_MS) return; //Ignore spurious pulses
        
        PulsePolarity = POL;
        
        char hadFraction = 0;
        if (PulsePolarity)
        {
            _fraction += _remainingMaSecsPerPulse;
            if (_fraction > 1000)
            {
                _fraction -= 1000;
                hadFraction = 1;
            }
            CountAddAmpSecondsU(_wholeAmpSecsPerPulse + hadFraction);
        }
        else
        {
            _fraction -= _remainingMaSecsPerPulse;
            if (_fraction < -1000)
            {
                _fraction += 1000;
                hadFraction = 1;
            }
            CountSubAmpSecondsU(_wholeAmpSecsPerPulse + hadFraction);
        }
        PulseInterval = interval;
        PulseMsCount = count;
    }
}
