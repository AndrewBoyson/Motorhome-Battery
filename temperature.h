#include <stdint.h>
extern int16_t TemperatureConvert8bfdpToTenths(int16_t fixed);
extern int16_t TemperatureConvertTenthsTo8bfdp(int16_t tenths);

extern char    TemperatureIsValid;
extern int16_t TemperatureGetAs8bfdp(void);
extern int16_t TemperatureGetAsTenths(void);

extern    void TemperatureMain(void);