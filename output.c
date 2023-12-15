
#include <xc.h>
#include <stdint.h>

#include "../mstimer.h"
#include "../eeprom.h"

#include "count.h"
#include "temperature.h"
#include "eeprom-this.h"
#include "voltage.h"

#define CHARGE     PORTBbits.RB5
#define SPARE_1    PORTBbits.RB1
#define SUPPLY_OFF PORTCbits.RC7

#define MIN_CHARGE_TEMPERATURE_8_BIT ( 5 << 8)
#define MAX_CHARGE_MV    (3500 * 4) //100%
#define MIN_DISCHARGE_MV (2500 * 4) //0%

#define STATE_NEUTRAL   0
#define STATE_CHARGE    1
#define STATE_DISCHARGE 2

char    _state = STATE_NEUTRAL;
char    _allowed = 0;
char    _chargeEnabled    = 0;
char    _dischargeEnabled = 0;
uint8_t _targetSoc      = 0;

char OutputGetState()
{
    switch (_state)
    {
        case STATE_NEUTRAL:   return 'N';
        case STATE_CHARGE:    return _allowed ? 'C' : 'c';
        case STATE_DISCHARGE: return _allowed ? 'D' : 'd';
        default:              return '?';
    }
}

static void saveEnables()
{
    uint8_t byte = 0;
    if (_chargeEnabled   ) byte |= 2;
    if (_dischargeEnabled) byte |= 1;
    EepromSaveU8(EEPROM_OUTPUT_ENABLES_U8, byte);
}

char    OutputGetChargeEnabled   () { return _chargeEnabled;    } void OutputSetChargeEnabled   (char    v) { _chargeEnabled    = v; saveEnables(); }
char    OutputGetDischargeEnabled() { return _dischargeEnabled; } void OutputSetDischargeEnabled(char    v) { _dischargeEnabled = v; saveEnables(); }
uint8_t OutputGetTargetSoc       () { return _targetSoc;        } void OutputSetTargetSoc       (uint8_t v) { _targetSoc        = v; EepromSaveU8(EEPROM_OUTPUT_TARGET_SOC_U8, _targetSoc); } 

void OutputInit()
{
    TRISB5 = 0;	//RB5 (pin 26) output  CHARGE
    TRISB1 = 0;	//RB1 (pin 22) output
    TRISC7 = 0;	//RC7 (pin 18) output  DISCHARGE
    
    CHARGE    = 0;
    SUPPLY_OFF = 0;
    SPARE_1   = 0;
    
    uint8_t byte = EepromReadU8(EEPROM_OUTPUT_ENABLES_U8);
    _chargeEnabled    = byte & 2;
    _dischargeEnabled = byte & 1;
    _targetSoc = EepromReadU8(EEPROM_OUTPUT_TARGET_SOC_U8);
}

void OutputMain()
{
//    uint8_t soc = CountGetSocPercent();
//    uint8_t target = GetTargetSoc();
//    uint8_t hys    = GetTargetHys();
    /*
Suppose target is 50% or 140Ah
Start charge when SoC falls below 50.0% - 
Start discharge when SoC reaches 51.0%
Start neutral when SoC reaches 50.9%
     */
    uint32_t            socAmpSeconds = CountGetAmpSeconds();
    uint32_t         targetAmpSeconds = (uint32_t)OutputGetTargetSoc() * 280 * 36;
    uint32_t    chargeStartAmpSeconds = targetAmpSeconds - (uint32_t)4999 * 28 * 36 / 1000; //49.501 = 50 - 0.499% 
    uint32_t dischargeStartAmpSeconds = targetAmpSeconds + (uint32_t)4999 * 28 * 36 / 1000; //50.499 = 50 + 0.499%

    switch (_state)
    {
        case STATE_NEUTRAL:
            if (socAmpSeconds >= dischargeStartAmpSeconds) _state = STATE_DISCHARGE; //Drifts up to 50.499% but in practice only here if the target is changed
            if (socAmpSeconds <=    chargeStartAmpSeconds) _state = STATE_CHARGE;    //Drifts down to 49.501%
            break;
        case STATE_CHARGE:
            if (socAmpSeconds >=         targetAmpSeconds) _state = STATE_NEUTRAL; //Charges to 50%
            break;
        case STATE_DISCHARGE:
            if (socAmpSeconds <=         targetAmpSeconds) _state = STATE_NEUTRAL; //Discharges to 50%
            break;
    }

    switch (_state)
    {
        case STATE_NEUTRAL:
            CHARGE     = 0;
            SUPPLY_OFF = 0;
            break;
        case STATE_CHARGE:
            _allowed = (TemperatureGetAs8bfdp() >= MIN_CHARGE_TEMPERATURE_8_BIT) &&
                       (VoltageGetAsMv() < MAX_CHARGE_MV) &&
                       _chargeEnabled;
            CHARGE     = _allowed;
            SUPPLY_OFF = 0;
            break;
        case STATE_DISCHARGE:
            _allowed = (VoltageGetAsMv()     > MIN_DISCHARGE_MV) &&
                       _dischargeEnabled;
            CHARGE     = 0;
            SUPPLY_OFF = _allowed;
            break;
    }
}