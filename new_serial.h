/* The length of the queue used to send messages to the LCD gatekeeper task. */
#define uartQUEUE_SIZE		3
/*-----------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "serial.h"

xComPortHandle xSerialPortInitMinimal( unsigned long ulWantedBaud, unsigned portBASE_TYPE uxQueueLength );
signed portBASE_TYPE xSerialGetChar( xComPortHandle pxPort, signed char *pcRxedChar, portTickType xBlockTime );
signed portBASE_TYPE xSerialGetChar( xComPortHandle pxPort, signed char *pcRxedChar, portTickType xBlockTime );
signed portBASE_TYPE xSerialPutChar( xComPortHandle pxPort, signed char cOutChar, portTickType xBlockTime );
void vSerialClose( xComPortHandle xPort );
void vSerialPutString( xComPortHandle pxPort, const signed char * const pcString, unsigned short usStringLength );
