#include <stdint.h>
#include <xc.h>

#include "../mstimer.h"

#define OVERSAMPLE_RATE 14

uint16_t _value = 0;
char     AdcValueIsValid = 0;

uint16_t AdcGetValue()
{
    uint16_t value;
    di();
        value = _value;
    ei();
    return value;
}

char AdcHadInterrupt()
{
    return ADIF;
}
void AdcHandleInterrupt()
{
    static uint32_t total = 0;
    static uint16_t count = 0;
    
    uint16_t value = ((uint16_t)ADRESH << 8) + ADRESL;
    total += value;    //Will contain 12 bit value * 4096 ==> 24 bit value
    count++;
//    if (count >= 4096)
    if (count >= (1 << OVERSAMPLE_RATE))
    {
        count = 0;
//        _value = (uint16_t)(total >> 8); //Decimate to 16 bits
        _value = (uint16_t)(total >> (12 + OVERSAMPLE_RATE - 16)); //Decimate to 16 bits
        AdcValueIsValid = 1;
        total = 0;
    }
    ADCON0bits.GO = 1; //Start conversion
    ADIF = 0;          //Clear the interrupt bit
}
void AdcInit(void)
{
	ADCON2bits.ADFM  = 1; // Right justified into lsb
	ADCON2bits.ACQT  = 7; // 20TAD
    
    ADCON2bits.ADCS  = 2; //TAD = 32 Tosc - A bit more than 16 recommended for 20MHz
	
	ADCON1bits.VCFG  = 3; //Vr+ uses internal 4.096 reference
	ADCON1bits.VNCFG = 0; //Vr- uses AVss
	ADCON1bits.CHSN  = 3; //-ve input <-- AN2
	
	ANCON0            = 0;
	ANCON0bits.ANSEL4 = 0;//AN4 configured as digital
	ANCON0bits.ANSEL3 = 0;//AN3 configured as digital
	ANCON0bits.ANSEL2 = 1;//AN2 configured as analogue
	ANCON0bits.ANSEL1 = 1;//AN1 configured as analogue
	ANCON0bits.ANSEL0 = 0;//AN0 configured as digital
    ANCON1            = 0;//AN8-14 configured as digital
			
    ADCON0bits.CHS    = 1; //+ve input <-- AN1
    
	ADCON0bits.ADON  = 1; //Enable adc
    
    ADIF = 0;             //Clear the interrupt bit
    ADIE = 1;             //Enable interrupts
    ADCON0bits.GO = 1;    //Start conversion
    
}