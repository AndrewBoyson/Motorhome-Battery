#include <stdint.h>
#include <xc.h>

#include "../mstimer.h"

#define KP1 PORTAbits.RA5
#define KP2 PORTAbits.RA3
#define KP3 PORTAbits.RA6
#define KP4 PORTAbits.RA7

#define MULTIPLY_MS   3000
#define PRE_REPEAT_MS 1000
#define REPEAT_MS      200
#define DEBOUNCE_MS     50

static char _pressed1   = 0;
static char _pressed2   = 0;
static char _pressed3   = 0;
static char _pressed4   = 0;

char KeypadMultiplier = 0; //0 = 1; 1 = 10; 2 = 100; 3 = 1000
char KeypadOneShot    = 0;

void KeypadTickHandler()
{
    static int8_t debounceMs1 = 0;
    static int8_t debounceMs2 = 0;
    static int8_t debounceMs3 = 0;
    static int8_t debounceMs4 = 0;

    if (!KP1 && debounceMs1 <  DEBOUNCE_MS) debounceMs1++;
    if ( KP1 && debounceMs1 >            0) debounceMs1--;
    if (        debounceMs1 == 0          ) _pressed1 = 0;
    if (        debounceMs1 == DEBOUNCE_MS) _pressed1 = 1;
    
    if (!KP2 && debounceMs2 <  DEBOUNCE_MS) debounceMs2++;
    if ( KP2 && debounceMs2 >            0) debounceMs2--;
    if (        debounceMs2 == 0          ) _pressed2 = 0;
    if (        debounceMs2 == DEBOUNCE_MS) _pressed2 = 1;
    
    if (!KP3 && debounceMs3 <  DEBOUNCE_MS) debounceMs3++;
    if ( KP3 && debounceMs3 >            0) debounceMs3--;
    if (        debounceMs3 == 0          ) _pressed3 = 0;
    if (        debounceMs3 == DEBOUNCE_MS) _pressed3 = 1;
    
    if (!KP4 && debounceMs4 <  DEBOUNCE_MS) debounceMs4++;
    if ( KP4 && debounceMs4 >            0) debounceMs4--;
    if (        debounceMs4 == 0          ) _pressed4 = 0;
    if (        debounceMs4 == DEBOUNCE_MS) _pressed4 = 1;
}

void KeypadMain()
{
    static char kpWasPressed1 = 0;
    static char kpWasPressed2 = 0;
    static char kpWasPressed3 = 0;
    static char kpWasPressed4 = 0;
    static uint32_t msTimerRepeat = 0;
    static uint32_t msTimerPreRepeat = 0;
    static uint32_t msTimerMultiply = 0;
    
    char kp1 = _pressed1;
    char kp2 = _pressed2;
    char kp3 = _pressed3;
    char kp4 = _pressed4;
    
    KeypadOneShot = 0;
    if (kp1 && !kpWasPressed1) KeypadOneShot |= 1;
    if (kp2 && !kpWasPressed2) KeypadOneShot |= 2;
    if (kp3 && !kpWasPressed3) KeypadOneShot |= 4;
    if (kp4 && !kpWasPressed4) KeypadOneShot |= 8;
    
    kpWasPressed1 = kp1;
    kpWasPressed2 = kp2;
    kpWasPressed3 = kp3;
    kpWasPressed4 = kp4;
    
    if (!kp1 && !kp2 && !kp3 && !kp4)
    {
        KeypadMultiplier = 0;
        msTimerRepeat    = MsTimerCount;
        msTimerPreRepeat = MsTimerCount;
        msTimerMultiply  = MsTimerCount;
    }
    if (MsTimerRelative(msTimerPreRepeat, PRE_REPEAT_MS) && MsTimerRelative(msTimerRepeat, REPEAT_MS))
    {
        kpWasPressed1 = 0;
        kpWasPressed2 = 0;
        kpWasPressed3 = 0;
        kpWasPressed4 = 0;
        msTimerRepeat = MsTimerCount;
    }
    if (MsTimerRelative(msTimerMultiply, MULTIPLY_MS))
    {
        msTimerMultiply = MsTimerCount;
        KeypadMultiplier++;
        if (KeypadMultiplier > 3) KeypadMultiplier = 3;
    }
}