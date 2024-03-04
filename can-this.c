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
        case CAN_ID_SERVER  + CAN_ID_TIME:                MsTickerRegulate         (*(uint32_t*)pData); break;
        case CAN_ID_BATTERY + CAN_ID_COUNTED_AMP_SECONDS: CountSetAmpSeconds       (*(uint32_t*)pData); break;
        case CAN_ID_BATTERY + CAN_ID_OUTPUT_TARGET:       OutputSetTargetSoc       (*( uint8_t*)pData); break;
        case CAN_ID_BATTERY + CAN_ID_CHARGE_ENABLED:      OutputSetChargeEnabled   (*    (char*)pData); break;
        case CAN_ID_BATTERY + CAN_ID_DISCHARGE_ENABLED:   OutputSetDischargeEnabled(*    (char*)pData); break;
        case CAN_ID_BATTERY + CAN_ID_HEATER_TARGET:       HeaterSetTargetTenths    (*( int16_t*)pData); break;
        case CAN_ID_BATTERY + CAN_ID_AGING_AS_PER_HOUR:   CountSetAgingAsPerHour   (*( int16_t*)pData); break;
        case CAN_ID_BATTERY + CAN_ID_HEATER_PROPORTIONAL: HeaterSetKp8bfdp         (*(uint16_t*)pData); break;
        case CAN_ID_BATTERY + CAN_ID_HEATER_INTEGRAL:     HeaterSetKi8bfdp         (*(uint16_t*)pData); break;
    }
}

void CanThisInit(void)
{
    CanReceive = &receive;
}
void CanThisMain(void)
{
    { uint32_t value = CountGetAmpSeconds       (); static struct CanTransmitState state; CanTransmitOnChange(&state, CAN_ID_BATTERY, CAN_ID_COUNTED_AMP_SECONDS, sizeof(value), &value); }
    {  int32_t value = PulseGetCurrentMa        (); static struct CanTransmitState state; CanTransmitOnChange(&state, CAN_ID_BATTERY, CAN_ID_MA                 , sizeof(value), &value); }
    
    {  uint8_t value = OutputGetTargetSoc       (); static struct CanTransmitState state; CanTransmitOnChange(&state, CAN_ID_BATTERY, CAN_ID_OUTPUT_TARGET      , sizeof(value), &value); }
    {     char value = OutputGetState           (); static struct CanTransmitState state; CanTransmitOnChange(&state, CAN_ID_BATTERY, CAN_ID_OUTPUT_STATE       , sizeof(value), &value); }
    {     char value = OutputGetChargeEnabled   (); static struct CanTransmitState state; CanTransmitOnChange(&state, CAN_ID_BATTERY, CAN_ID_CHARGE_ENABLED     , sizeof(value), &value); }
    {     char value = OutputGetDischargeEnabled(); static struct CanTransmitState state; CanTransmitOnChange(&state, CAN_ID_BATTERY, CAN_ID_DISCHARGE_ENABLED  , sizeof(value), &value); }
    
    {  int16_t value = TemperatureGetAs8bfdp    (); static struct CanTransmitState state; CanTransmitOnChange(&state, CAN_ID_BATTERY, CAN_ID_TEMPERATURE_8BFDP  , sizeof(value), &value); }
    {  int16_t value = HeaterGetTargetTenths    (); static struct CanTransmitState state; CanTransmitOnChange(&state, CAN_ID_BATTERY, CAN_ID_HEATER_TARGET      , sizeof(value), &value); }
    {  uint8_t value = HeaterGetOutputFixed     (); static struct CanTransmitState state; CanTransmitOnChange(&state, CAN_ID_BATTERY, CAN_ID_HEATER_OUTPUT      , sizeof(value), &value); }
    { uint16_t value = HeaterGetKp8bfdp         (); static struct CanTransmitState state; CanTransmitOnChange(&state, CAN_ID_BATTERY, CAN_ID_HEATER_PROPORTIONAL, sizeof(value), &value); }
    { uint16_t value = HeaterGetKi8bfdp         (); static struct CanTransmitState state; CanTransmitOnChange(&state, CAN_ID_BATTERY, CAN_ID_HEATER_INTEGRAL    , sizeof(value), &value); }
    
    {  int16_t value = VoltageGetAsMv           (); static struct CanTransmitState state; CanTransmitOnChange(&state, CAN_ID_BATTERY, CAN_ID_VOLTAGE            , sizeof(value), &value); }
    {  int16_t value = CountGetAgingAsPerHour   (); static struct CanTransmitState state; CanTransmitOnChange(&state, CAN_ID_BATTERY, CAN_ID_AGING_AS_PER_HOUR  , sizeof(value), &value); }
}
