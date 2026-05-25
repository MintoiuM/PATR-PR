/*
    FreeRTOS V6.0.4 - Copyright (C) 2010 Real Time Engineers Ltd.
*/

/* Scheduler includes. */
#include "FreeRTOS.h"

/* Demo app includes. */
#include "partest.h"

//#define ptOUTPUT 	0			// toti pinii sint iesiri
#define ptOUTPUT 	0x04			// RB2 - intrare DS18S20
#define ptALL_OFF	0
//#define ptALL_OFF	0xFFFF		// modificat, toate iesirile in 1

unsigned portBASE_TYPE uxOutput;

/*-----------------------------------------------------------
 * Simple parallel port IO routines.
 *-----------------------------------------------------------*/

void vParTestInitialise( void )
{
	CNPU1 = 0;	//	Change on notification pins pull-up resistors disabled
	CNPU2 = 0;	//
	TRISB = ptOUTPUT;		// modificat din A in B
	PORTB = ptALL_OFF;		// modificat din A in B
	uxOutput = ptALL_OFF;
}
/*-----------------------------------------------------------*/

void vParTestSetLED( unsigned portBASE_TYPE uxLED, signed portBASE_TYPE xValue )
{
unsigned portBASE_TYPE uxLEDBit;

	/* Which port A bit is being modified? */
	uxLEDBit = 1 << uxLED;

	if( xValue )
	{
		/* Turn the LED on. */
		portENTER_CRITICAL();
		{
			uxOutput |= uxLEDBit;
			PORTB = uxOutput;		// modificat din A in B
		}
		portEXIT_CRITICAL();
	}
	else
	{
		/* Turn the LED off. */
		portENTER_CRITICAL();
		{
			uxOutput &= ~uxLEDBit;
			PORTB = uxOutput;		// modificat din A in B
		}
		portEXIT_CRITICAL();
	}
}
/*-----------------------------------------------------------*/

void vParTestToggleLED( unsigned portBASE_TYPE uxLED )
{
unsigned portBASE_TYPE uxLEDBit;

	uxLEDBit = 1 << uxLED;
	portENTER_CRITICAL();
	{
		/* If the LED is already on - turn it off.  If the LED is already
		off, turn it on. */
		if( uxOutput & uxLEDBit )
		{
			uxOutput &= ~uxLEDBit;
			PORTB = uxOutput;		// modificat din A in B
		}
		else
		{
			uxOutput |= uxLEDBit;
			PORTB = uxOutput;		// modificat din A in B
		}
	}

	portEXIT_CRITICAL();
}

