#include <stdint.h>

extern int16_t  HeaterGetTargetTenths(void);
extern void     HeaterSetTargetTenths(int16_t value);

extern  int8_t  HeaterGetOffsetPercent(void);
extern uint8_t  HeaterGetOutputPercent(void);
extern uint8_t  HeaterGetOutputFixed  (void);

extern uint16_t HeaterGetKp8bfdp(void);
extern void     HeaterSetKp8bfdp(uint16_t value);

extern uint16_t HeaterGetKi8bfdp(void);
extern void     HeaterSetKi8bfdp(uint16_t value);

extern void HeaterInit(void);
extern void HeaterMain(void);