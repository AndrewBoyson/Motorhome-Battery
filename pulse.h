
#include <stdint.h>

extern uint32_t PulseInterval;
extern char     PulsePolarity;
extern char     PulsePolarityInst;

extern uint16_t PulseGetMilliAmpSecondsEach(void);
extern void     PulseSetMilliAmpSecondsEach(uint16_t v);

extern void     PulseInit(void);
extern void     PulseMain(void);
extern uint32_t PulseGetAbsoluteCurrentMa(void);
extern int32_t  PulseGetCurrentMa(void);
extern uint32_t PulseGetMsSinceLastPulse(void);
