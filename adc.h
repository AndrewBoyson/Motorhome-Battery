
extern void AdcInit(void);
extern char AdcHadInterrupt(void);
extern void AdcHandleInterrupt(void);


extern uint16_t AdcGetValue(void);
extern char AdcValueIsValid;

#define ADC_VREF_MV 4096
#define ADC_VREF 4.096f
#define ADC_BITS 16
