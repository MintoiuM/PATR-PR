/* SFR that seems to be missing from the standard header files. */
//#define PMAEN				*( ( unsigned short * ) 0x60c )

/*-----------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

void delayus(int us);
void delayms(int ms);
void LCD_DATA_OR(int val);
void LCD_DATA_AND(int val);
void clear(void);
void send_char2LCD(char ax);
void LCD_line(int linie);
void LCD_init(void);
void LCD_printf(char *text);
void LCD_Goto(int linie, int col);
void LCD_On_Off(int On_Off);
