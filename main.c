#include <xc.h>

#include "../reset.h"
#include "../hrtimer.h"
#include "../msticker.h"
#include "../mstimer.h"
#include "../can.h"
#include "../i2c.h"
#include "../lcd-1602.h"

#include "adc.h"
#include "display.h"
#include "pulse.h"
#include "count.h"
#include "temperature.h"
#include "keypad.h"
#include "output.h"
#include "heater.h"
#include "can-this.h"
#include "eeprom-this.h"
#include "i2c-this.h"

#define _XTAL_FREQ 8000000

void __interrupt() isr(void)
{
    if (MsTickerHadInterrupt())
    {
        MsTimerTickHandler();
        KeypadTickHandler();
        MsTickerHandleInterrupt();
    }
    if (AdcHadInterrupt())
    {
        AdcHandleInterrupt();
    }
}

void main(void)
{
    __delay_ms(3000); //This prevents multiple resets when programming.
    ResetInit();
    HrTimerInit();
    MsTickerInit(EEPROM_MS_TICK_COUNT_U16);
    AdcInit();
    I2CInit();
    CountInit();
    PulseInit();
    OutputInit();
    HeaterInit();
    LcdInit(I2C_ADDRESS_LCD);
    DisplayInit();
    CanInit();
    CanThisInit();
    
    ei();
    PEIE = 1; //Enable peripheral interrupts - specifically Timer 1 and ADC
    
	while(1)
	{
        MsTimerMain();
        PulseMain();
        CountMain();
        TemperatureMain();
        OutputMain();
        HeaterMain();
        KeypadMain();
        LcdMain();
        DisplayMain();
        CanMain();
        CanThisMain();
    }
}
