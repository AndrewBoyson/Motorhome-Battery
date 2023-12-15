extern char OutputGetState(void);

extern char    OutputGetChargeEnabled   (void); extern void OutputSetChargeEnabled   (char    v);
extern char    OutputGetDischargeEnabled(void); extern void OutputSetDischargeEnabled(char    v);
extern uint8_t OutputGetTargetSoc       (void); extern void OutputSetTargetSoc       (uint8_t v);

extern void OutputInit(void);
extern void OutputMain(void);