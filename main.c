/* ============================================================
 * TEMA PROIECT - PATR
 * dsPIC33FJ128MC802 + FreeRTOS
 *
 * PIN MAP (PORTB):
 *  RB15-RB12  LCD data (D7-D4)
 *  RB11       LED status aplicatie (placa PATR: active-low, 0=aprins)
 *             aplicatie pornita -> aprins continuu; oprita -> intermitent
 *  RB10       PWM1H3  -> semnal servo
 *  RB9        UART1 RX (RS232)
 *  RB8        UART1 TX (RS232)
 *  RB7        INT0    -> buton S2 (pornire/oprire aplicatie)
 *  RB6        LCD RS
 *  RB5        LCD RW
 *  RB4        LCD E
 *  RB3        AN5     -> potentiometru (mod manual)
 *  RB2        1-wire  -> DS18B20
 *  RB1        LED mod de lucru (placa PATR: active-low; automat=aprins=0, manual=stins=1)
 * ============================================================*/

/* Standard includes */
#include <stdio.h>
#include <string.h>

/* Scheduler includes */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "croutine.h"

/* Demo application includes */
#include "partest.h"

/* Proprii */
#include "new_lcd.h"
#include "new_serial.h"
#include "ds18b20.h"

/* ----------------------------------------------------------------
 * Configurare oscilator
 * ----------------------------------------------------------------*/
_FOSCSEL(FNOSC_FRC);
_FOSC(FCKSM_CSECMD & OSCIOFNC_OFF);
_FWDT(FWDTEN_OFF);

/* ----------------------------------------------------------------
 * Definitii generale
 * ----------------------------------------------------------------*/
#define mainCOM_TEST_BAUD_RATE      ( 9600 )
#define comBUFFER_LEN               ( 120 )
#define comNO_BLOCK                 ( ( portTickType ) 0 )
#define comRX_BLOCK_TIME            ( ( portTickType ) 0xffff )

/* LED-uri */
#define LED_STATUS      _RB11
#define LED_MODE        _RB1
#define TRIS_LED_STATUS _TRISB11
#define TRIS_LED_MODE   _TRISB1

/* RB1 LED mod: acelasi tip active-low ca RB11; 0 pe pin = LED fizic aprins */
#define LED_MODE_AUTO_PIN    0
#define LED_MODE_MANUAL_PIN  1

/* Moduri de lucru */
#define MODE_AUTO   0
#define MODE_MANUAL 1

/* Servo PWM (edge-aligned, PTMOD=free-run):
 * Fcy=40MHz, prescaler /64 => 1 tick PWM = 1.6us
 * Perioada ~20ms: P1TPER = 12500
 * Puls 1ms   (0 deg):   DC = 625
 * Puls 1.5ms (90 deg):  DC = 938
 * Puls 2ms   (180 deg): DC = 1250
 */
#define PWM_PERIOD  12500
#define PWM_CENTER  938
#define PWM_MIN     625
#define PWM_MAX     1250

/* ----------------------------------------------------------------
 * Variabile globale partajate
 * ----------------------------------------------------------------*/
static volatile float g_temperatura   = 25.0f;
static volatile int   g_tensiune_x100 = 200;
static volatile int   g_mod_lucru     = MODE_AUTO;
static volatile int   g_app_pornita   = 1;
static volatile char  g_last_cmd[4]   = "---";

/* Semafoare binare folosite ca mutex
 * (nu necesita configUSE_MUTEXES) */
static xSemaphoreHandle xDataSem;
static xSemaphoreHandle xLCDSem;

/* ----------------------------------------------------------------
 * Prototipuri
 * ----------------------------------------------------------------*/
static void prvSetupHardware(void);
static void initPLL(void);
static void initPWM(void);
static void initAdc(void);
static void initTimer3(void);
static void setServoDC(unsigned int dc);

static void TaskTemp     (void *params);
static void TaskServo    (void *params);
static void TaskSerial   (void *params);
static void TaskLCD      (void *params);
static void TaskLEDStatus(void *params);

/* ----------------------------------------------------------------
 * INT0 ISR - buton S2 pe RB7, front descrescator
 * Debounce: ignora apasarile repetate in fereastra de 200ms
 * ----------------------------------------------------------------*/
void __attribute__((interrupt, no_auto_psv)) _INT0Interrupt(void)
{
    static volatile unsigned int last_tick = 0;
    unsigned int now = (unsigned int)xTaskGetTickCount();

    /* Accepta apasarea doar daca au trecut cel putin 200ms de la ultima */
    if ((unsigned int)(now - last_tick) >= (unsigned int)(200 / portTICK_RATE_MS))
    {
        last_tick = now;

        g_app_pornita = !g_app_pornita;

        if (!g_app_pornita)
            setServoDC(PWM_CENTER);
    }

    _INT0IF = 0;
}

/* ----------------------------------------------------------------
 * ADC ISR - citeste potentiometrul de pe RB3 (AN5)
 * ----------------------------------------------------------------*/
void __attribute__((interrupt, no_auto_psv)) _ADC1Interrupt(void)
{
    unsigned int val_adc = ADC1BUF0;
    g_tensiune_x100 = (int)((unsigned long)val_adc * 330UL / 4096UL);
    _AD1IF = 0;
}

/* ================================================================
 * TASK: citire temperatura DS18B20 (la fiecare 1s)
 * ================================================================*/
static void TaskTemp(void *params)
{
    float t;
    (void)params;

    for (;;)
    {
        if (g_app_pornita)
        {
            t = ds1820_read();
            if (xSemaphoreTake(xDataSem, ( portTickType ) 10) == pdTRUE)
            {
                g_temperatura = t;
                xSemaphoreGive(xDataSem);
            }
        }
        vTaskDelay(1000 / portTICK_RATE_MS);
    }
}

/* ================================================================
 * TASK: control servo
 * Mod automat: pozitia in functie de temperatura (20-30 grade C)
 * Mod manual:  pozitia in functie de tensiune  (1-3V)
 * ================================================================*/
static void TaskServo(void *params)
{
    float        temp = 25.0f;
    float        voltage = 2.0f;
    unsigned int pwmDutyCycle = PWM_CENTER;
    (void)params;

    for (;;)
    {
        if (g_app_pornita)
        {
            int mod_cur = g_mod_lucru;
            int tens_cur = g_tensiune_x100;
            voltage = (float)tens_cur / 100.0f;

            if (xSemaphoreTake(xDataSem, ( portTickType ) 10) == pdTRUE)
            {
                temp = g_temperatura;
                xSemaphoreGive(xDataSem);
            }

            if (mod_cur == MODE_AUTO)
            {
                if (temp <= 20.0f) pwmDutyCycle = PWM_MIN;
                else if (temp >= 30.0f) pwmDutyCycle = PWM_MAX;
                else
                {
                    pwmDutyCycle = PWM_MIN + (unsigned int)(((temp - 20.0f) * (PWM_MAX - PWM_MIN)) / 10.0f);
                }
            }
            else
            {
                if (voltage <= 1.0f) pwmDutyCycle = PWM_MIN;
                else if (voltage >= 3.0f) pwmDutyCycle = PWM_MAX;
                else
                {
                    pwmDutyCycle = PWM_MIN + (unsigned int)(((voltage - 1.0f) * (PWM_MAX - PWM_MIN)) / 2.0f);
                }
            }

            setServoDC(pwmDutyCycle);
        }
        else
        {
            setServoDC(PWM_CENTER);
        }

        /* Cerinta: automat = LED aprins, manual = LED stins (hardware active-low). */
        LED_MODE = (g_mod_lucru == MODE_AUTO) ? LED_MODE_AUTO_PIN : LED_MODE_MANUAL_PIN;

        vTaskDelay(100 / portTICK_RATE_MS);
    }
}

/* ================================================================
 * TASK: comunicatie seriala RS232
 * Comenzi (1 caracter):
 *   'm' -> interogare mod de lucru
 *   's' -> comutare mod auto/manual
 *   't' -> interogare temperatura
 * ================================================================*/
static void TaskSerial(void *params)
{
    signed char cByteRxed;
    char        raspuns[60];
    float       temp = 25.0f;
    int         mod  = MODE_AUTO;
    (void)params;

    vTaskDelay(500 / portTICK_RATE_MS);
    vSerialPutString(NULL, (const signed char * const)"\r\n=== MENU ===\r\n", comNO_BLOCK);
    vSerialPutString(NULL, (const signed char * const)" m - mod de lucru\r\n", comNO_BLOCK);
    vSerialPutString(NULL, (const signed char * const)" s - comutare auto/manual\r\n", comNO_BLOCK);
    vSerialPutString(NULL, (const signed char * const)" t - temperatura\r\n", comNO_BLOCK);
    vSerialPutString(NULL, (const signed char * const)"=============\r\n", comNO_BLOCK);

    for (;;)
    {
        if (xSerialGetChar(NULL, &cByteRxed, comRX_BLOCK_TIME))
        {
            if (xSemaphoreTake(xDataSem, ( portTickType ) 10) == pdTRUE)
            {
                g_last_cmd[0] = (char)cByteRxed;
                g_last_cmd[1] = '\0';
                temp = g_temperatura;
                mod  = g_mod_lucru;
                xSemaphoreGive(xDataSem);
            }

            switch (cByteRxed)
            {
                case 'm':
                    sprintf(raspuns, "Mod: %s\r\n",
                            (mod == MODE_AUTO) ? "AUTOMAT" : "MANUAL");
                    vSerialPutString(NULL,
                        (const signed char * const)raspuns, comNO_BLOCK);
                    break;

                case 's':
                    /* Comutare robusta: pentru un int volatil nu depindem de semafor.
                     * Daca Take expira, modul trebuie totusi sa se schimbe imediat. */
                    g_mod_lucru = (g_mod_lucru == MODE_AUTO) ?
                                   MODE_MANUAL : MODE_AUTO;
                    mod = g_mod_lucru;
                    sprintf(raspuns, "Comutare -> %s\r\n",
                            (mod == MODE_AUTO) ? "AUTOMAT" : "MANUAL");
                    vSerialPutString(NULL,
                        (const signed char * const)raspuns, comNO_BLOCK);
                    break;

                case 't':
                {
                    int t_int, t_frac;
                    if (temp < 0.0f)
                    {
                        t_int  = -(int)(-temp);
                        t_frac = (int)((-temp - (int)(-temp)) * 100);
                    }
                    else
                    {
                        t_int  = (int)temp;
                        t_frac = (int)((temp - (int)temp) * 100);
                    }
                    sprintf(raspuns, "Temperatura: %d.%02d C\r\n",
                            t_int, t_frac);
                    vSerialPutString(NULL,
                        (const signed char * const)raspuns, comNO_BLOCK);
                    break;
                }

                default:
                    vSerialPutString(NULL, (const signed char * const)"Comanda necunoscuta!\r\n", comNO_BLOCK);
				    vSerialPutString(NULL, (const signed char * const)"\r\n=== MENU ===\r\n", comNO_BLOCK);
				    vSerialPutString(NULL, (const signed char * const)" m - mod de lucru\r\n", comNO_BLOCK);
				    vSerialPutString(NULL, (const signed char * const)" s - comutare auto/manual\r\n", comNO_BLOCK);
				    vSerialPutString(NULL, (const signed char * const)" t - temperatura\r\n", comNO_BLOCK);
				    vSerialPutString(NULL, (const signed char * const)"=============\r\n", comNO_BLOCK);
                    break;
            }
        }
    }
}

/* ================================================================
 * TASK: afisare LCD (refresh la 500ms)
 * Linia 1: Temp: XX.XX C
 * Linia 2: Mod: AUTO/MANUAL
 * Linia 3: V: X.XXV
 * Linia 4: Cmd: X
 * ================================================================*/
static void TaskLCD(void *params)
{
    char  buf[21];
    float temp = 25.0f;
    int   tens = 200;
    int   mod  = MODE_AUTO;
    char  last[4];
    int   t_int, t_frac, v_int, v_frac;
    (void)params;

    last[0] = '-'; last[1] = '-'; last[2] = '-'; last[3] = '\0';

    for (;;)
    {
        if (xSemaphoreTake(xDataSem, ( portTickType ) 20) == pdTRUE)
        {
            temp    = g_temperatura;
            tens    = g_tensiune_x100;
            mod     = g_mod_lucru;
            last[0] = g_last_cmd[0];
            last[1] = g_last_cmd[1];
            last[2] = g_last_cmd[2];
            last[3] = '\0';
            xSemaphoreGive(xDataSem);
        }

        if (temp < 0.0f)
        {
            t_int  = -(int)(-temp);
            t_frac = (int)((-temp - (int)(-temp)) * 100);
        }
        else
        {
            t_int  = (int)temp;
            t_frac = (int)((temp - (int)temp) * 100);
        }
        v_int  = tens / 100;
        v_frac = tens % 100;

        if (xSemaphoreTake(xLCDSem, ( portTickType ) 20) == pdTRUE)
        {
            LCD_Goto(1, 1);
            if (g_app_pornita)
                sprintf(buf, "Temp:%3d.%02d C      ", t_int, t_frac);
            else
                sprintf(buf, "Temp: -- OPRIT --   ");
            LCD_printf(buf);

            LCD_Goto(2, 1);
            sprintf(buf, "Mod: %-14s",
                    (mod == MODE_AUTO) ? "AUTO" : "MANUAL");
            LCD_printf(buf);

            LCD_Goto(3, 1);
            sprintf(buf, "V: %d.%02dV           ", v_int, v_frac);
            LCD_printf(buf);

            LCD_Goto(4, 1);
            sprintf(buf, "Cmd: %-15s", last);
            LCD_printf(buf);

			LCD_Goto(3, 15);
			LCD_printf("Dibu &");

			LCD_Goto(4, 14);
			LCD_printf("Mintoiu");

            xSemaphoreGive(xLCDSem);
        }

        vTaskDelay(500 / portTICK_RATE_MS);
    }
}

/* ================================================================
 * TASK: LED status aplicatie (RB11, active-low pe placa)
 *   pornita -> LED aprins constant (pin = 0)
 *   oprita  -> intermitent la 500ms
 * ================================================================*/
static void TaskLEDStatus(void *params)
{
    (void)params;

    for (;;)
    {
        if (g_app_pornita)
        {
            LED_STATUS = 0;
            vTaskDelay(100 / portTICK_RATE_MS);
        }
        else
        {
            LED_STATUS = (int)LED_STATUS ^ 1;
            vTaskDelay(500 / portTICK_RATE_MS);
        }
    }
}

/* ================================================================
 * MAIN
 * ================================================================*/
int main(void)
{
    prvSetupHardware();

    /* vSemaphoreCreateBinary: disponibil in FreeRTOS v6 fara
     * configUSE_MUTEXES. Semafor creat in stare "dat" (=1),
     * deci primul Take va trece imediat - comportament corect
     * pentru un mutex simplu. */
    vSemaphoreCreateBinary(xDataSem);
    vSemaphoreCreateBinary(xLCDSem);

    xTaskCreate(TaskTemp,      (signed portCHAR *)"Temp", configMINIMAL_STACK_SIZE * 2, NULL, tskIDLE_PRIORITY + 2, NULL);
    xTaskCreate(TaskServo,     (signed portCHAR *)"Serv", configMINIMAL_STACK_SIZE,     NULL, tskIDLE_PRIORITY + 3, NULL);
    xTaskCreate(TaskSerial,    (signed portCHAR *)"Ser",  configMINIMAL_STACK_SIZE * 2, NULL, tskIDLE_PRIORITY + 2, NULL);
    xTaskCreate(TaskLCD,       (signed portCHAR *)"LCD",  configMINIMAL_STACK_SIZE * 2, NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(TaskLEDStatus, (signed portCHAR *)"LED",  configMINIMAL_STACK_SIZE,     NULL, tskIDLE_PRIORITY + 1, NULL);

    vTaskStartScheduler();
    return 0;
}

/* ================================================================
 * Initializare hardware
 * ================================================================*/
static void initPLL(void)
{
    PLLFBD = 41;
    CLKDIVbits.PLLPOST = 0;
    CLKDIVbits.PLLPRE  = 0;
    __builtin_write_OSCCONH(0x01);
    __builtin_write_OSCCONL(0x01);
    while (OSCCONbits.COSC != 0b001);
    while (OSCCONbits.LOCK != 1);
}

/* PWM1H3 pe RB10, prescaler /64, T~20ms, centru~1.5ms */
static void initPWM(void)
{
    P1FLTACON = 0;
    P1OVDCON  = 0x3F00;

    P1TCON = 0;
    P1TMR  = 0;

    P1TCONbits.PTCKPS = 3;
    P1TCONbits.PTMOD  = 0;

    P1TPER = PWM_PERIOD;
    P1DC3  = PWM_CENTER;

    PWM1CON1 = 0;
    PWM1CON1bits.PMOD3 = 1;
    PWM1CON1bits.PEN3H = 1;
    PWM1CON1bits.PEN3L = 0;

    PWM1CON2 = 0;
    PWM1CON2bits.IUE = 1;

    _TRISB10 = 0;
    _LATB10  = 0;

    P1TCONbits.PTEN = 1;
}

/* ADC: AN5 (RB3), 12-bit, startat de T3 */
static void initAdc(void)
{
    AD1CON1bits.AD12B  = 1;
    AD1CON1bits.SSRC   = 2;
    AD1CON1bits.ASAM   = 1;
    AD1CON2bits.CSCNA  = 1;
    AD1CON3bits.ADRC   = 0;
    AD1CON3bits.ADCS   = 63;

    AD1CSSLbits.CSS5   = 1;
    AD1PCFGL           = 0xFFFF;
    AD1PCFGLbits.PCFG5 = 0;
    TRISBbits.TRISB3   = 1;

    _AD1IF = 0;
    _AD1IE = 1;
    AD1CON1bits.ADON   = 1;
}

static void initTimer3(void)
{
    TMR3 = 0;
    PR3  = 50000;
    T3CONbits.TON = 1;
}

static void prvSetupHardware(void)
{
    ADPCFG = 0xFFFF;
    PORTB  = 0x0000;
    TRISB  = 0x0000;

    initPLL();

    TRIS_LED_STATUS = 0;
    TRIS_LED_MODE   = 0;
    /* RB11 / RB1: active-low; 0 = aprins. Mod implicit AUTO -> RB1 aprins. */
    LED_STATUS = 0;
    LED_MODE   = LED_MODE_AUTO_PIN;

    _TRISB7 = 1;
    _INT0IF  = 0;
    _INT0IE  = 1;
    _INT0EP  = 1;
    _INT0IP  = 6;

    CNPU1 = 0x0040;
    output_float();
    ONE_WIRE_PIN = 1;

    initPWM();
    initAdc();
    initTimer3();

    LCD_init();
    LCD_line(1); LCD_printf("Temp: --");
    LCD_line(2); LCD_printf("Mod: AUTO");
    LCD_line(3); LCD_printf("V: --");
    LCD_line(4); LCD_printf("Cmd: --");

    xSerialPortInitMinimal(mainCOM_TEST_BAUD_RATE, comBUFFER_LEN);
}

static void setServoDC(unsigned int dc)
{
    P1DC3 = dc;
}

void vApplicationIdleHook(void)
{
}