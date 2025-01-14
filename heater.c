#include <xc.h>
#include "../mstimer.h"
#include "../eeprom.h"

#include "temperature.h"
#include "eeprom-this.h"

 int16_t _targetTenths         = 0;
uint16_t  _kp8bfdp             = 0;
uint16_t  _ki8bfdp             = 0;
 int32_t _integralOutput16bfdp = 0;
uint8_t  _power0to255          = 0;
 
 /*
  Heater resistors are wired in series parallel and are 8.8 ohms in total (8.8 ohms each)
  So a 14 volt (charging voltage) will consume 1.6 amp and provide 22 watts of heating.
  It takes about 10 watts to lift the temperature by 10 degrees so 1 degree per watt
  
  The PWM output turns on when TMR2 matches PR2.
  The PWM output turns off when TMR2 and 2 bits from the prescaler matches CCPR3L and CCP3CONbits.DC3B
  
  The control system output _heaterOutputFixed is an unsigned 8 bit value so PR2 should be 0x3F.
  
  Process variable    (pv)              degrees             as a    signed 16bit with an 8 bit fixed decimal point DDDD FFFF
  Set point           (sp)              degrees             as a    signed 16bit with an 8 bit fixed decimal point DDDD FFFF
  Error               (sp-pv)           degrees             as a    signed 16bit with an 8 bit fixed decimal point DDDD FFFF
  Proportional gain   (kp)              power/degree        as an unsigned  8bit with an 8 bit fixed decimal point      FFFF
  Integral     gain   (ki)              power/degree/minute as an unsigned  8bit with an 8 bit fixed decimal point      FFFF
  Proportional output (error x kp)      power               as a    signed 24bit with a 16 bit fixed decimal point DDDD FFFF FFFF
  Integral     output (sum(error x ki)) power               as a    signed 24bit with a 16 bit fixed decimal point DDDD FFFF FFFF
  Output                                power               as an unsigned  8bit 0-255, 0-100% 0-22W               DDDD
  
 */

int16_t  HeaterGetTargetTenths () { return _targetTenths; }
 int8_t  HeaterGetOffsetPercent() { return ( int8_t)(_integralOutput16bfdp * 100 / 256 / 256 / 256); }
uint8_t  HeaterGetOutputPercent() { return (uint8_t)(((uint16_t) _power0to255 + 1) * 100 / 256); } // 1 corresponds to about 0.5%
uint8_t  HeaterGetOutputFixed  () { return _power0to255; }
uint16_t HeaterGetKp8bfdp      () { return _kp8bfdp; }
uint16_t HeaterGetKi8bfdp      () { return _ki8bfdp; }

void HeaterSetTargetTenths(int16_t  value) { _targetTenths = value; EepromSaveS16(EEPROM_HEATER_TARGET_TENTHS_S16, value); }
void HeaterSetKp8bfdp     (uint16_t value) { _kp8bfdp      = value; EepromSaveU16(EEPROM_HEATER_KP_U16           , value); }
void HeaterSetKi8bfdp     (uint16_t value) { _ki8bfdp      = value; EepromSaveU16(EEPROM_HEATER_KI_U16           , value); }

void HeaterInit(void)
{
    TRISC6 = 0;	//RC6 (pin 17) output
    C3TSEL = 0; //Associate CCP3 module with Timer 2
    CCP3CONbits.CCP3M = 0xC; //PWM mode
    
    PR2 = 0x3F; //Set Timer 2 preset compare to 6 of the 8 bits
    TMR2ON = 1; //Turn on Timer 2
    
    _targetTenths         =          EepromReadS16(EEPROM_HEATER_TARGET_TENTHS_S16);
    _kp8bfdp              =          EepromReadU16(EEPROM_HEATER_KP_U16           );
    _ki8bfdp              =          EepromReadU16(EEPROM_HEATER_KI_U16           );
    _integralOutput16bfdp = (int32_t)EepromReadS8 (EEPROM_HEATER_OUTPUT_OFFSET_S8 ) * 256 * 256;
}
static const uint8_t _sqrt[] = {
    0, 16, 23, 28, 32, 36, 39, 42, 45, 48, 51, 53, 55, 58, 60, 62,
   64, 66, 68, 70, 72, 73, 75, 77, 78, 80, 82, 83, 85, 86, 88, 89,
   91, 92, 93, 95, 96, 97, 99,100,101,102,104,105,106,107,109,110,
  111,112,113,114,115,116,118,119,120,121,122,123,124,125,126,127,
  128,129,130,131,132,133,134,135,136,137,138,139,139,140,141,142,
  143,144,145,146,147,148,148,149,150,151,152,153,153,154,155,156,
  157,158,158,159,160,161,162,162,163,164,165,166,166,167,168,169,
  169,170,171,172,172,173,174,175,175,176,177,177,178,179,180,180,
  181,182,182,183,184,185,185,186,187,187,188,189,189,190,191,191,
  192,193,193,194,195,195,196,197,197,198,199,199,200,200,201,202,
  202,203,204,204,205,206,206,207,207,208,209,209,210,210,211,212,
  212,213,213,214,215,215,216,216,217,218,218,219,219,220,221,221,
  222,222,223,223,224,225,225,226,226,227,227,228,229,229,230,230,
  231,231,232,232,233,234,234,235,235,236,236,237,237,238,238,239,
  239,240,241,241,242,242,243,243,244,244,245,245,246,246,247,247,
  248,248,249,249,250,250,251,251,252,252,253,253,254,254,255,255 };

void setPwmDutyCycle(uint8_t dutyCycle)
{
    CCP3CONbits.DC3B = dutyCycle >> 0; //Set to low 2 bits of the desired duty cycle
    CCPR3L           = dutyCycle >> 2; //Set to the high 6 bits of the desired duty cycle
}
#define MAX_INT_24 (int32_t)0x007FFFFF // 127 * 256 * 256
#define MIN_INT_24 (int32_t)0xFF800000 //-128 * 256 * 256

void HeaterMain(void)
{
    //Run only when there is a new sample
    if (!TemperatureIsValid) return;
    if (!TemperatureSampleIsReadyForUseByHeater) return;
    TemperatureSampleIsReadyForUseByHeater = 0;
    
    //static uint32_t msTimerIntegralDo   = 0;
    static uint32_t msTimerIntegralSave = 0;
    //char doIntegral   = MsTimerRepetitive(&msTimerIntegralDo  ,  60000); //Every minute
    char saveIntegral = MsTimerRepetitive(&msTimerIntegralSave, 600000); //Every 10 minutes
    
    //if (!doIntegral) return;
    
    int16_t sp8bfdp    = TemperatureConvertTenthsTo8bfdp(_targetTenths);
    int16_t pv8bfdp    = TemperatureGetAs8bfdp();
    int16_t error8bfdp = sp8bfdp - pv8bfdp;

    //Proportional
    int32_t      proportionalOutput16bfdp  = (int32_t)error8bfdp * _kp8bfdp;

    //Integral
    _integralOutput16bfdp += (int32_t)error8bfdp * _ki8bfdp;
    
    //Output
    int32_t output16bfdp = proportionalOutput16bfdp + _integralOutput16bfdp;
    if (output16bfdp > MAX_INT_24) output16bfdp = MAX_INT_24;
    if (output16bfdp < MIN_INT_24) output16bfdp = MIN_INT_24;
    _integralOutput16bfdp = output16bfdp - proportionalOutput16bfdp; //If the output has been limited then adjust integral to match
    
    //Set duty cycle
    _power0to255 = (uint8_t)(output16bfdp / 256 / 256 + 128); //-128 + 128 = 0; 127 + 128 = 255
    setPwmDutyCycle(_sqrt[_power0to255]);
    
    //Save integral
    if (saveIntegral) EepromSaveS8(EEPROM_HEATER_OUTPUT_OFFSET_S8, (int8_t)(_integralOutput16bfdp / 256 / 256));
}