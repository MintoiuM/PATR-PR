
/* Standard include file. */
#include <stdlib.h>

/* Scheduler include files. */
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

/* Demo app include files. */
#include "serial.h"
#include "new_serial.h"
#include "p33Fxxxx.h"

/* Hardware setup. */
#define serOUTPUT						0
#define serINPUT						1
#define serLOW_SPEED					0
#define serONE_STOP_BIT					0
#define serEIGHT_DATA_BITS_NO_PARITY	0
#define serNORMAL_IDLE_STATE			0
#define serAUTO_BAUD_OFF				0
#define serLOOPBACK_OFF					0
#define serWAKE_UP_DISABLE				0
#define serNO_HARDWARE_FLOW_CONTROL		0
#define serSTANDARD_IO					0
#define serNO_IRDA						0
#define serCONTINUE_IN_IDLE_MODE		0
#define serUART_ENABLED					1
#define serINTERRUPT_ON_SINGLE_CHAR		0
#define serTX_ENABLE					1
#define serINTERRUPT_ENABLE				1
#define serINTERRUPT_DISABLE			0
#define serCLEAR_FLAG					0
#define serSET_FLAG						1


/* The queues used to communicate between tasks and ISR's. */
static xQueueHandle xRxedChars; 
static xQueueHandle xCharsForTx; 

static portBASE_TYPE xTxHasEnded;
/*-----------------------------------------------------------*/

xComPortHandle xSerialPortInitMinimal( unsigned long ulWantedBaud, unsigned portBASE_TYPE uxQueueLength )
{
char cChar;

	TRISBbits.TRISB9 = 1; 	//set RB9 input pin
	//LATB=0;
	PORTBbits.RB8 = 1; 	// set portb

	// Setare pini RB8 si RB9 ca pini pentru UART1
	RPOR4 = 3; 		// U1TX connected at RB8 pin out
	RPINR18 = 9; 	// U1RX set RB9 pin in

	/* Create the queues used by the com test task. */
	xRxedChars = xQueueCreate( uxQueueLength, ( unsigned portBASE_TYPE ) sizeof( signed char ) );
	xCharsForTx = xQueueCreate( uxQueueLength, ( unsigned portBASE_TYPE ) sizeof( signed char ) );

	/* Setup the UART. */
	U1MODEbits.BRGH		= serLOW_SPEED;
	U1MODEbits.STSEL	= serONE_STOP_BIT;
	U1MODEbits.PDSEL	= serEIGHT_DATA_BITS_NO_PARITY;
	U1MODEbits.ABAUD	= serAUTO_BAUD_OFF;
	U1MODEbits.LPBACK	= serLOOPBACK_OFF;
	U1MODEbits.WAKE		= serWAKE_UP_DISABLE;
	U1MODEbits.UEN		= serNO_HARDWARE_FLOW_CONTROL;
	U1MODEbits.IREN		= serNO_IRDA;
	U1MODEbits.USIDL	= serCONTINUE_IN_IDLE_MODE;
	U1MODEbits.UARTEN	= serUART_ENABLED;

	U1BRG = (unsigned short)(( (float)configCPU_CLOCK_HZ / ( (float)16 * (float)ulWantedBaud ) ) - (float)0.5);

	U1STAbits.URXISEL	= serINTERRUPT_ON_SINGLE_CHAR;
	U1STAbits.UTXEN		= serTX_ENABLE;
	U1STAbits.UTXINV	= serNORMAL_IDLE_STATE;
	U1STAbits.UTXISEL0	= serINTERRUPT_ON_SINGLE_CHAR;
	U1STAbits.UTXISEL1	= serINTERRUPT_ON_SINGLE_CHAR;

	/* It is assumed that this function is called prior to the scheduler being
	started.  Therefore interrupts must not be allowed to occur yet as they
	may attempt to perform a context switch. */
	portDISABLE_INTERRUPTS();

	IFS0bits.U1RXIF = serCLEAR_FLAG;
	IFS0bits.U1TXIF = serCLEAR_FLAG;
	IPC2bits.U1RXIP = configKERNEL_INTERRUPT_PRIORITY;
	IPC3bits.U1TXIP = configKERNEL_INTERRUPT_PRIORITY;
	IEC0bits.U1TXIE = serINTERRUPT_ENABLE;
	IEC0bits.U1RXIE = serINTERRUPT_ENABLE;

	/* Clear the Rx buffer. */
	while( U1STAbits.URXDA == serSET_FLAG )
	{
		cChar = U1RXREG;
	}

	xTxHasEnded = pdTRUE;

	return NULL;
}
/*-----------------------------------------------------------*/

signed portBASE_TYPE xSerialGetChar( xComPortHandle pxPort, signed char *pcRxedChar, portTickType xBlockTime )
{
	/* Only one port is supported. */
	( void ) pxPort;

	/* Get the next character from the buffer.  Return false if no characters
	are available or arrive before xBlockTime expires. */
	if( xQueueReceive( xRxedChars, pcRxedChar, xBlockTime ) )
	{
		return pdTRUE;
	}
	else
	{
		return pdFALSE;
	}
}
/*-----------------------------------------------------------*/

signed portBASE_TYPE xSerialPutChar( xComPortHandle pxPort, signed char cOutChar, portTickType xBlockTime )
{
	/* Only one port is supported. */
	( void ) pxPort;

	/* Return false if after the block time there is no room on the Tx queue. */
	if( xQueueSend( xCharsForTx, &cOutChar, xBlockTime ) != pdPASS )
	{
		return pdFAIL;
	}

	/* A critical section should not be required as xTxHasEnded will not be
	written to by the ISR if it is already 0 (is this correct?). */
	if( xTxHasEnded )
	{
		xTxHasEnded = pdFALSE;
		IFS0bits.U1TXIF = serSET_FLAG;
	}

	return pdPASS;
}
/*-----------------------------------------------------------*/

void vSerialClose( xComPortHandle xPort )
{
}
/*-----------------------------------------------------------*/

void __attribute__((__interrupt__, auto_psv)) _U1RXInterrupt( void )
{
char cChar;
portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;

	/* Get the character and post it on the queue of Rxed characters.
	If the post causes a task to wake force a context switch as the woken task
	may have a higher priority than the task we have interrupted. */
	IFS0bits.U1RXIF = serCLEAR_FLAG;
	while( U1STAbits.URXDA )
	{
		cChar = U1RXREG;
		xQueueSendFromISR( xRxedChars, &cChar, &xHigherPriorityTaskWoken );
	}

	if( xHigherPriorityTaskWoken != pdFALSE )
	{
		taskYIELD();
	}
}
/*-----------------------------------------------------------*/

void __attribute__((__interrupt__, auto_psv)) _U1TXInterrupt( void )
{
signed char cChar;
portBASE_TYPE xTaskWoken = pdFALSE;

	/* If the transmit buffer is full we cannot get the next character.
	Another interrupt will occur the next time there is space so this does
	not matter. */
	IFS0bits.U1TXIF = serCLEAR_FLAG;
	while( !( U1STAbits.UTXBF ) )
	{
		if( xQueueReceiveFromISR( xCharsForTx, &cChar, &xTaskWoken ) == pdTRUE )
		{
			/* Send the next character queued for Tx. */
			U1TXREG = cChar;
		}
		else
		{
			/* Queue empty, nothing to send. */
			xTxHasEnded = pdTRUE;
			break;
		}
	}

	if( xTaskWoken != pdFALSE )
	{
		taskYIELD();
	}
}

// adaugata din alt port.....
void vSerialPutString( xComPortHandle pxPort, const signed char * const pcString, unsigned short usStringLength )
{
signed char *pxNext;

	/* A couple of parameters that this port does not use. */
	( void ) usStringLength;
	( void ) pxPort;

	/* NOTE: This implementation does not handle the queue being full as no block time is used! */

	/* The port handle is not required as this driver only supports UART0. */
	( void ) pxPort;

	/* Send each character in the string, one at a time. */
	pxNext = ( signed char * ) pcString;
	while( *pxNext )
	{
		//xSerialPutChar( pxPort, *pxNext, serNO_BLOCK );
		xSerialPutChar( pxPort, *pxNext, 0 );	// comNO_BLOCK
		pxNext++;
	}
}

