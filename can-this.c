#include "../mstimer.h"
#include "../msticker.h"
#include "../can.h"
#include "../canids.h"

#include "count.h"
#include "pulse.h"
#include "output.h"
#include "temperature.h"
#include "heater.h"
#include "voltage.h"

#define BASE_MS 1000

static void receive(uint16_t id, uint8_t length, void* pData)
{
    switch(id)
    {
        case CAN_ID_SERVER  + CAN_ID_TIME:                       MsTickerRegulate           (*(uint32_t*)pData); break;
        case CAN_ID_BATTERY + CAN_ID_COUNTED_AMP_SECONDS:        CountSetAmpSeconds         (*(uint32_t*)pData); break;
        case CAN_ID_BATTERY + CAN_ID_OUTPUT_SETPOINT:            OutputSetTargetSoc         (*( uint8_t*)pData); break;
        case CAN_ID_BATTERY + CAN_ID_CHARGE_ENABLED:             OutputSetChargeEnabled     (*    (char*)pData); break;
        case CAN_ID_BATTERY + CAN_ID_DISCHARGE_ENABLED:          OutputSetDischargeEnabled  (*    (char*)pData); break;
        case CAN_ID_BATTERY + CAN_ID_HEATER_SET_POINT:           HeaterSetTargetTenths      (*( int16_t*)pData); break;
        case CAN_ID_BATTERY + CAN_ID_AGING_AS_PER_HOUR:          CountSetAgingAsPerHour     (*( int16_t*)pData); break;
        case CAN_ID_BATTERY + CAN_ID_HEATER_PROPORTIONAL:        HeaterSetKp8bfdp           (*(uint16_t*)pData); break;
        case CAN_ID_BATTERY + CAN_ID_HEATER_INTEGRAL:            HeaterSetKi8bfdp           (*(uint16_t*)pData); break;
    }
}

void CanThisInit(void)
{
    CanReceive = &receive;
}
void CanThisMain(void)
{
    static uint32_t msTimerAmpSeconds      = 0;
    static uint32_t msTimerMa              = 0;
    static uint32_t msOutputSetPoint       = 0;
    static uint32_t msOutputState          = 0;
    static uint32_t msChargeEnabled        = 0;
    static uint32_t msDischargeEnabled     = 0;
    static uint32_t msTemperature8bfdp     = 0;
    static uint32_t msTemperatureSetPoint  = 0;
    static uint32_t msHeaterOutput         = 0;
    static uint32_t msHeaterKp             = 0;
    static uint32_t msHeaterKi             = 0;
    static uint32_t msVoltage              = 0;
    static uint32_t msTimerAging           = 0;
    
    
    static char sendAmpSeconds          = 0;
    static char sendMa                  = 0;
    static char sendOutputSetPoint      = 0;
    static char sendOutputState         = 0;
    static char sendChargeEnabled       = 0;
    static char sendDischargeEnabled    = 0;
    static char sendTemperature8bfdp    = 0;
    static char sendTemperatureSetPoint = 0;
    static char sendHeaterOutput        = 0;
    static char sendHeaterKp            = 0;
    static char sendHeaterKi            = 0;
    static char sendVoltage             = 0;
    static char sendAging               = 0;
    
    if (MsTimerRepetitive(&msTimerAmpSeconds     , BASE_MS + CAN_ID_COUNTED_AMP_SECONDS       )) sendAmpSeconds          = 1;
    if (MsTimerRepetitive(&msTimerMa             , BASE_MS + CAN_ID_MA                        )) sendMa                  = 1;
    if (MsTimerRepetitive(&msOutputSetPoint      , BASE_MS + CAN_ID_OUTPUT_SETPOINT           )) sendOutputSetPoint      = 1;
    if (MsTimerRepetitive(&msOutputState         , BASE_MS + CAN_ID_OUTPUT_STATE              )) sendOutputState         = 1;
    if (MsTimerRepetitive(&msChargeEnabled       , BASE_MS + CAN_ID_CHARGE_ENABLED            )) sendChargeEnabled       = 1;
    if (MsTimerRepetitive(&msDischargeEnabled    , BASE_MS + CAN_ID_DISCHARGE_ENABLED         )) sendDischargeEnabled    = 1;
    if (MsTimerRepetitive(&msTemperature8bfdp    , BASE_MS + CAN_ID_TEMPERATURE_8BFDP         )) sendTemperature8bfdp    = 1;
    if (MsTimerRepetitive(&msTemperatureSetPoint , BASE_MS + CAN_ID_HEATER_SET_POINT          )) sendTemperatureSetPoint = 1;
    if (MsTimerRepetitive(&msHeaterOutput        , BASE_MS + CAN_ID_HEATER_OUTPUT             )) sendHeaterOutput        = 1;
    if (MsTimerRepetitive(&msHeaterKp            , BASE_MS + CAN_ID_HEATER_PROPORTIONAL       )) sendHeaterKp            = 1;
    if (MsTimerRepetitive(&msHeaterKi            , BASE_MS + CAN_ID_HEATER_INTEGRAL           )) sendHeaterKi            = 1;
    if (MsTimerRepetitive(&msVoltage             , BASE_MS + CAN_ID_VOLTAGE                   )) sendVoltage             = 1;
    if (MsTimerRepetitive(&msTimerAging          , BASE_MS + CAN_ID_AGING_AS_PER_HOUR         )) sendAging               = 1;
    
    if (sendAmpSeconds         ) { uint32_t ampSeconds          = CountGetAmpSeconds()         ; sendAmpSeconds          = CanTransmit(CAN_ID_BATTERY + CAN_ID_COUNTED_AMP_SECONDS     , sizeof(ampSeconds         ), &ampSeconds         ); }
    if (sendMa                 ) {  int32_t mA                  = PulseGetCurrentMa()          ; sendMa                  = CanTransmit(CAN_ID_BATTERY + CAN_ID_MA                      , sizeof(mA                 ), &mA                 ); }
    if (sendOutputSetPoint     ) {  uint8_t outputSetPoint      = OutputGetTargetSoc()         ; sendOutputSetPoint      = CanTransmit(CAN_ID_BATTERY + CAN_ID_OUTPUT_SETPOINT         , sizeof(outputSetPoint     ), &outputSetPoint     ); }
    if (sendOutputState        ) {     char outputState         = OutputGetState()             ; sendOutputState         = CanTransmit(CAN_ID_BATTERY + CAN_ID_OUTPUT_STATE            , sizeof(outputState        ), &outputState        ); }
    if (sendChargeEnabled      ) {     char chargeEnabled       = OutputGetChargeEnabled()     ; sendChargeEnabled       = CanTransmit(CAN_ID_BATTERY + CAN_ID_CHARGE_ENABLED          , sizeof(chargeEnabled      ), &chargeEnabled      ); }
    if (sendDischargeEnabled   ) {     char dischargeEnabled    = OutputGetDischargeEnabled()  ; sendDischargeEnabled    = CanTransmit(CAN_ID_BATTERY + CAN_ID_DISCHARGE_ENABLED       , sizeof(dischargeEnabled   ), &dischargeEnabled   ); }
    if (sendTemperature8bfdp   ) {  int16_t temperatureTenths   = TemperatureGetAs8bfdp()      ; sendTemperature8bfdp    = CanTransmit(CAN_ID_BATTERY + CAN_ID_TEMPERATURE_8BFDP       , sizeof(temperatureTenths  ), &temperatureTenths  ); }
    if (sendTemperatureSetPoint) {  int16_t heaterSetPoint      = HeaterGetTargetTenths()      ; sendTemperatureSetPoint = CanTransmit(CAN_ID_BATTERY + CAN_ID_HEATER_SET_POINT        , sizeof(heaterSetPoint     ), &heaterSetPoint     ); }
    if (sendHeaterOutput       ) {  uint8_t heaterOutput        = HeaterGetOutputFixed()       ; sendHeaterOutput        = CanTransmit(CAN_ID_BATTERY + CAN_ID_HEATER_OUTPUT           , sizeof(heaterOutput       ), &heaterOutput       ); }
    if (sendHeaterKp           ) { uint16_t heaterKp            = HeaterGetKp8bfdp()           ; sendHeaterKp            = CanTransmit(CAN_ID_BATTERY + CAN_ID_HEATER_PROPORTIONAL     , sizeof(heaterKp           ), &heaterKp           ); }
    if (sendHeaterKi           ) { uint16_t heaterKi            = HeaterGetKi8bfdp()           ; sendHeaterKi            = CanTransmit(CAN_ID_BATTERY + CAN_ID_HEATER_INTEGRAL         , sizeof(heaterKi           ), &heaterKi           ); }
    if (sendVoltage            ) {  int16_t mv                  = VoltageGetAsMv()             ; sendVoltage             = CanTransmit(CAN_ID_BATTERY + CAN_ID_VOLTAGE                 , sizeof(mv                 ), &mv                 ); }
    if (sendAging              ) {  int16_t agingAsPerHour      = CountGetAgingAsPerHour()     ; sendAging               = CanTransmit(CAN_ID_BATTERY + CAN_ID_AGING_AS_PER_HOUR       , sizeof(agingAsPerHour     ), &agingAsPerHour     ); }
}
