#include "new_lcd.h"
#include "FreeRTOS.h"
#include "task.h"
#include "string.h"	// pentru functia strlen()
#include "queue.h"

////////////////////////////////PENTRU LCD//////////////////////////////////////////
#define LCD_ON _RB7
#define LCD_RS _RB6
#define LCD_RW _RB5
#define LCD_E  _RB4
#define LCD_BL _RB3

// Intirziere in microsecunde
void delayus(int us){
int i;
for(i=0;i<us;i++)
	{
	asm("nop"); asm("nop"); asm("nop"); asm("nop"); asm("nop"); 
	asm("nop"); asm("nop"); asm("nop"); asm("nop"); asm("nop"); 
	asm("nop"); asm("nop"); asm("nop"); asm("nop"); asm("nop"); 
	asm("nop"); asm("nop"); asm("nop"); asm("nop"); asm("nop");
	asm("nop"); asm("nop"); asm("nop"); asm("nop"); asm("nop"); 
	asm("nop"); asm("nop"); asm("nop"); asm("nop"); asm("nop"); 
	asm("nop"); asm("nop"); asm("nop"); asm("nop"); asm("nop"); 
    }
}

// Intirziere in ms
void delayms(int ms)
{
	int i;
	for(i=0;i<ms;i++)   delayus(1000);
}

/* RB15-RB12 = date LCD. Folosim LATB (nu PORTB): RMW pe PORTB citea RB10 (PWM)
 * la nivel instantaneu si corupea semnalul servo pe acelasi port. */
void LCD_DATA_OR(int val)
{
	LATB |= (unsigned int)((unsigned int)val << 12);
}

void LCD_DATA_AND(int val)
{
	LATB &= (unsigned int)(((unsigned int)val << 12) | 0xFFFu);
}

// Sterge ecranul LCD
void clear(void)
{
	LCD_DATA_OR(0x00);
	LCD_E = 1; delayus(40); LCD_E = 0;
	LCD_DATA_AND(~0x00);

	LCD_DATA_OR(0x01);
	LCD_E = 1; delayms(2); LCD_E = 0;
	LCD_DATA_AND(~0x01); 

}

// Trimite un caracter catre LCD
void send_char2LCD(char ax){
	LCD_DATA_OR(ax>>4);
	LCD_RS = 1;	
	delayus(1); LCD_E = 1;	delayus(40); LCD_E = 0; 
	LCD_DATA_AND(~(ax>>4)); 
	LCD_DATA_OR(ax&0x0F);
	delayus(1); LCD_E = 1;	delayus(40); LCD_E = 0; 
	LCD_RS = 0;
	LCD_DATA_AND(~(ax&0x0F)); 
	delayus(40);
}

// Deplaseaza cursorul la inceputul liniei indicate
void LCD_line(int linie)
{
	char ax=0;
	switch(linie)
	{
	case 1: ax = 0x80; break;
	case 2: ax = 0xC0; break;
	case 3: ax = 0x94; break;
	case 4: ax = 0xD4; break;
	}
	LCD_DATA_OR(ax>>4);
	LCD_E = 1; delayus(20); LCD_E = 0;
	LCD_DATA_AND(~(ax>>4)); 
	LCD_DATA_OR(ax&0x0F);
	LCD_E = 1; delayus(20); LCD_E = 0;
	LCD_DATA_AND(~(ax&0x0F)); 
}

// Initializeaza LCD-ul
void LCD_init(void)
{
	// Alimenteaza LCD-ul
	LCD_ON = 1;
	delayms(40);
	
	// Function set
	LCD_DATA_OR(0x02);
	LCD_E = 1; delayus(40); LCD_E = 0;
	LCD_DATA_AND(~0x02); 

	LCD_DATA_OR(0x02);
	LCD_E = 1; delayus(40); LCD_E = 0;
	LCD_DATA_AND(~0x02); 

	LCD_DATA_OR(0x08);	// afisaj LCD cu doua linii (adica 4!!)
	LCD_E = 1; delayus(40); LCD_E = 0;
	LCD_DATA_AND(~0x08); 

	// Display, cursor and blink on/off control
	LCD_DATA_OR(0x00);
	LCD_E = 1; delayus(40); LCD_E = 0;
	LCD_DATA_AND(~0x00); 

	LCD_DATA_OR(0x0E);
	LCD_E = 1; delayus(40); LCD_E = 0;
	LCD_DATA_AND(~0x0E); 

	// Display clear
	clear();
	// Entry mode set
	LCD_DATA_OR(0x00);
	LCD_E = 1; delayus(40); LCD_E = 0;
	LCD_DATA_AND(~0x00); 

	LCD_DATA_OR(0x06);
	LCD_E = 1; delayus(40); LCD_E = 0;
	LCD_DATA_AND(~0x06); 

	// Asteapta terminarea initializarii
	delayms(2);
}

// Afiseaza un sir de caractere la pozitia curenta a cursorului
void LCD_printf(char *text)
{
 	int i;
	for (i=0;i<strlen(text);i++)	send_char2LCD(*(text+i));
}

// Deplaseaza cursorul la linia si coloana indicata
void LCD_Goto(int linie, int col)
{
  char ax=0;
  switch(linie)
    {
    case 1: ax=0x80; break;
    case 2: ax=0xC0; break;
    case 3: ax=0x94; break;
    case 4: ax=0xD4; break;
    }
	ax = ax + col - 1;
	LCD_DATA_OR(ax>>4);
	LCD_E = 1; delayus(4); LCD_E = 0;
	LCD_DATA_AND(~(ax>>4)); 
	LCD_DATA_OR(ax&0x0F);
	LCD_E = 1; delayus(4); LCD_E = 0;
	LCD_DATA_AND(~(ax&0x0F)); 
}

// Controleaza afisarea pe ecran
void LCD_On_Off(int On_Off)
{
  char ax = 0;
  switch(On_Off)
  	{
    case 1: ax=0x0A; break;
    case 0: ax=0x08; break;
    } 
  LCD_DATA_OR(ax);
  LCD_E = 1; delayus(40); LCD_E = 0;
  LCD_DATA_AND(~ax);
  delayus(40);
}
