/*
    FreeRTOS V6.0.4 - Copyright (C) 2010 Real Time Engineers Ltd.
*/

#ifndef LCD_INC_H
#define LCD_INC_H

/* Create the task that will control the LCD.  Returned is a handle to the queue
on which messages to get written to the LCD should be written. */
xQueueHandle xStartLCDTask( void );

typedef struct
{
	/* The minimum amount of time the message should remain on the LCD without
	being overwritten. */
	portTickType xMinDisplayTime;

	/* A pointer to the string to be displayed. */
	char *pcMessage;

} xLCDMessage;


#endif /* LCD_INC_H */


