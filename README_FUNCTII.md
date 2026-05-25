# Explicație funcții — linie cu linie

Documentație pentru codul aplicației PATR (`main.c`, `new_lcd.c`, `new_serial.c`, `ds18b20.c`).  
Fiecare funcție este parcursă în ordinea liniilor din sursă.

---

## `main.c`

### `_INT0Interrupt` — ISR buton S2 (RB7)

| Linie | Cod | Explicație |
|-------|-----|------------|
| 115 | `void __attribute__((interrupt, no_auto_psv)) _INT0Interrupt(void)` | Handler de întrerupere pentru INT0; `no_auto_psv` evită salvarea automată PSV pe dsPIC. |
| 117 | `static volatile unsigned int last_tick = 0;` | Momentul ultimei apăsări acceptate (în tick-uri FreeRTOS), persistent între apeluri ISR. |
| 118 | `unsigned int now = (unsigned int)xTaskGetTickCount();` | Citește contorul de tick-uri curent al scheduler-ului. |
| 121 | `if ((unsigned int)(now - last_tick) >= ...` | Debounce: procesează doar dacă au trecut ≥ 200 ms de la ultima apăsare validă. |
| 121 | `200 / portTICK_RATE_MS` | Convertește 200 ms în număr de tick-uri (la 1 kHz → 200 tick-uri). |
| 123 | `last_tick = now;` | Actualizează referința de timp pentru următorul debounce. |
| 125 | `g_app_pornita = !g_app_pornita;` | Comută starea aplicației: pornit ↔ oprit. |
| 127-128 | `if (!g_app_pornita) setServoDC(PWM_CENTER);` | La oprire, mută imediat servo la 90° (puls ~1,5 ms). |
| 131 | `_INT0IF = 0;` | Resetează flag-ul de întrerupere INT0; obligatoriu ca să poată reveni ISR-ul. |

---

### `_ADC1Interrupt` — ISR conversie ADC

| Linie | Cod | Explicație |
|-------|-----|------------|
| 137 | `void __attribute__((interrupt, no_auto_psv)) _ADC1Interrupt(void)` | Handler la finalul unei conversii ADC1. |
| 139 | `unsigned int val_adc = ADC1BUF0;` | Citește rezultatul conversiei (12 biți) din bufferul ADC. |
| 140 | `g_tensiune_x100 = (int)((unsigned long)val_adc * 330UL / 4096UL);` | Scalează la sutimi de volt: V = ADC×3,3/4096; stochează de ex. 200 pentru 2,00 V. |
| 141 | `_AD1IF = 0;` | Curăță flag-ul de întrerupere ADC1. |

---

### `TaskTemp` — citire temperatură

| Linie | Cod | Explicație |
|-------|-----|------------|
| 147 | `static void TaskTemp(void *params)` | Task FreeRTOS pentru senzorul DS18B20. |
| 149 | `float t;` | Variabilă locală pentru temperatura citită. |
| 150 | `(void)params;` | Parametrul task-ului nefolosit; elimină avertisment compilator. |
| 152 | `for (;;)` | Buclă infinită — task-ul nu se termină niciodată. |
| 154 | `if (g_app_pornita)` | Citește senzorul doar când aplicația e activă. |
| 156 | `t = ds1820_read();` | Protocol 1-Wire: conversie + citire scratchpad → °C. |
| 157 | `if (xSemaphoreTake(xDataSem, 10) == pdTRUE)` | Încearcă mutex pe date partajate (timeout 10 tick-uri). |
| 159 | `g_temperatura = t;` | Actualizează temperatura globală sub protecție. |
| 160 | `xSemaphoreGive(xDataSem);` | Eliberează mutexul. |
| 163 | `vTaskDelay(1000 / portTICK_RATE_MS);` | Așteaptă 1 secundă până la următoarea citire. |

---

### `TaskServo` — PWM servo + LED mod

| Linie | Cod | Explicație |
|-------|-----|------------|
| 172 | `static void TaskServo(void *params)` | Task control unghi servo. |
| 174-176 | `float temp`, `voltage`, `unsigned int pwmDutyCycle` | Variabile pentru calcul PWM. |
| 177 | `(void)params;` | Parametru nefolosit. |
| 179 | `for (;;)` | Buclă infinită la 100 ms. |
| 181 | `if (g_app_pornita)` | Ramura activă: calculează unghi din senzor. |
| 183 | `int mod_cur = g_mod_lucru;` | Copiază modul curent (AUTO/MANUAL). |
| 184 | `int tens_cur = g_tensiune_x100;` | Copiază tensiunea potențiometrului (sutimi V). |
| 185 | `voltage = (float)tens_cur / 100.0f;` | Convertește la volți (ex. 200 → 2,0 V). |
| 187-191 | `xSemaphoreTake` / `temp = g_temperatura` / `Give` | Citește temperatura în siguranță față de `TaskTemp`. |
| 193 | `if (mod_cur == MODE_AUTO)` | Mod automat: mapare după temperatură. |
| 195 | `if (temp <= 20.0f) pwmDutyCycle = PWM_MIN;` | ≤20°C → 0° (puls 1 ms, DC=625). |
| 196 | `else if (temp >= 30.0f) pwmDutyCycle = PWM_MAX;` | ≥30°C → 180° (puls 2 ms, DC=1250). |
| 199 | `pwmDutyCycle = PWM_MIN + (unsigned int)(((temp - 20.0f) * ...` | Între 20–30°C: interpolare liniară PWM. |
| 202 | `else` | Ramura MANUAL. |
| 204 | `if (voltage <= 1.0f) pwmDutyCycle = PWM_MIN;` | ≤1 V → 0°. |
| 205 | `else if (voltage >= 3.0f) pwmDutyCycle = PWM_MAX;` | ≥3 V → 180°. |
| 208 | `pwmDutyCycle = PWM_MIN + ... ((voltage - 1.0f) * ...` | Între 1–3 V: interpolare liniară. |
| 212 | `setServoDC(pwmDutyCycle);` | Scrie duty-cycle în registrul PWM canal 3. |
| 214-217 | `else { setServoDC(PWM_CENTER); }` | Aplicație oprită: servo la centru (90°). |
| 220 | `LED_MODE = (g_mod_lucru == MODE_AUTO) ? ...` | LED RB1: 0=aprins (AUTO), 1=stins (MANUAL), active-low. |
| 222 | `vTaskDelay(100 / portTICK_RATE_MS);` | Perioadă task: 100 ms. |

---

### `TaskSerial` — comenzi RS232

| Linie | Cod | Explicație |
|-------|-----|------------|
| 233 | `static void TaskSerial(void *params)` | Task meniu și procesare UART. |
| 235-238 | `cByteRxed`, `raspuns[60]`, `temp`, `mod` | Buffer caracter primit, răspuns formatat, copii locale date. |
| 239 | `(void)params;` | Parametru nefolosit. |
| 241 | `vTaskDelay(500 / portTICK_RATE_MS);` | Așteaptă 500 ms după boot (UART/LCD să fie gata). |
| 242-246 | `vSerialPutString(..., "=== MENU ==="...` | Afișează meniul de comenzi pe serială la pornire. |
| 248 | `for (;;)` | Buclă infinită de așteptare comenzi. |
| 250 | `if (xSerialGetChar(NULL, &cByteRxed, comRX_BLOCK_TIME))` | Blochează până primește un caracter (timeout mare). |
| 252 | `if (xSemaphoreTake(xDataSem, 10) == pdTRUE)` | Protejează accesul la variabile globale. |
| 254-255 | `g_last_cmd[0] = (char)cByteRxed; g_last_cmd[1] = '\0';` | Salvează ultima comandă pentru LCD (`Cmd: X`). |
| 256-257 | `temp = g_temperatura; mod = g_mod_lucru;` | Copiază valori pentru răspunsuri. |
| 258 | `xSemaphoreGive(xDataSem);` | Eliberează mutex. |
| 261 | `switch (cByteRxed)` | Procesează comanda primită. |
| 263-268 | `case 'm':` + `sprintf` + `vSerialPutString` | Comandă mod: răspunde `AUTOMAT` sau `MANUAL`. |
| 270-280 | `case 's':` | Comută `g_mod_lucru` fără a depinde strict de semafor; confirmă pe serială. |
| 282-300 | `case 't':` | Formatează temperatura cu parte întreagă și 2 zecimale; suportă valori negative. |
| 285-289 | ramura `temp < 0` | Descompune temperatura negativă pentru afișare corectă. |
| 291-294 | ramura `else` | Descompune temperatura pozitivă. |
| 295-298 | `sprintf(..., "%d.%02d C\r\n", ...)` | Trimite răspunsul temperaturii. |
| 302-309 | `default:` | Comandă necunoscută + reafișare meniu. |

---

### `TaskLCD` — refresh afișaj

| Linie | Cod | Explicație |
|-------|-----|------------|
| 322 | `static void TaskLCD(void *params)` | Task actualizare LCD la 500 ms. |
| 324-329 | `buf[21]`, variabile locale | Buffer 20 caractere + 1 NUL; copii temp/tens/mod/cmd. |
| 330 | `(void)params;` | Parametru nefolosit. |
| 332 | `last[0]='-'; ... last[3]='\0';` | Valoare implicită comandă: `"---"`. |
| 334 | `for (;;)` | Buclă infinită. |
| 336-346 | `xSemaphoreTake(xDataSem)` + copieri + `Give` | Citește atomically datele partajate. |
| 348-357 | calcul `t_int`, `t_frac` | Descompune temperatura pentru format `XX.XX`. |
| 358-359 | `v_int = tens / 100; v_frac = tens % 100;` | Tensiune din sutimi → întreg + zecimale. |
| 361 | `if (xSemaphoreTake(xLCDSem, 20) == pdTRUE)` | Mutex exclusiv pentru acces hardware LCD. |
| 363 | `LCD_Goto(1, 1);` | Cursor linia 1, coloana 1. |
| 364-367 | `if (g_app_pornita) sprintf ... else "OPRIT"` | Linia 1: temperatură sau mesaj oprire. |
| 368 | `LCD_printf(buf);` | Scrie linia 1 pe LCD. |
| 370-373 | linia 2 `Mod: AUTO/MANUAL` | Afișează modul de lucru. |
| 375-377 | linia 3 `V: X.XXV` | Afișează tensiunea potențiometrului. |
| 379-381 | linia 4 `Cmd: X` | Ultima comandă serială. |
| 383-387 | `LCD_Goto` + nume autori | Text suplimentar pe LCD (semnătură proiect). |
| 389 | `xSemaphoreGive(xLCDSem);` | Eliberează LCD. |
| 392 | `vTaskDelay(500 / portTICK_RATE_MS);` | Refresh la 500 ms. |

---

### `TaskLEDStatus` — LED RB11

| Linie | Cod | Explicație |
|-------|-----|------------|
| 401 | `static void TaskLEDStatus(void *params)` | Task indicator stare aplicație. |
| 403 | `(void)params;` | Parametru nefolosit. |
| 405 | `for (;;)` | Buclă infinită. |
| 407 | `if (g_app_pornita)` | Aplicație pornită. |
| 409 | `LED_STATUS = 0;` | Active-low: pin 0 → LED aprins continuu. |
| 410 | `vTaskDelay(100 / portTICK_RATE_MS);` | Reîmprospătează la 100 ms (menține aprins). |
| 412 | `else` | Aplicație oprită. |
| 414 | `LED_STATUS = (int)LED_STATUS ^ 1;` | Comută bitul pinului → intermitență. |
| 415 | `vTaskDelay(500 / portTICK_RATE_MS);` | Perioadă blink: 500 ms. |

---

### `main`

| Linie | Cod | Explicație |
|-------|-----|------------|
| 423 | `int main(void)` | Punct de intrare după reset. |
| 425 | `prvSetupHardware();` | Inițializează PLL, GPIO, PWM, ADC, T3, LCD, UART. |
| 431 | `vSemaphoreCreateBinary(xDataSem);` | Creează semafor binar pentru date partajate (mutex simplu). |
| 432 | `vSemaphoreCreateBinary(xLCDSem);` | Creează semafor pentru acces exclusiv LCD. |
| 434 | `xTaskCreate(TaskTemp, ..., tskIDLE_PRIORITY + 2, ...)` | Task temperatură, stack 2× minimal, prioritate 2 peste idle. |
| 435 | `xTaskCreate(TaskServo, ..., + 3, ...)` | Task servo, prioritate maximă dintre task-uri aplicație. |
| 436 | `xTaskCreate(TaskSerial, ..., + 2, ...)` | Task UART, prioritate 2. |
| 437 | `xTaskCreate(TaskLCD, ..., + 1, ...)` | Task LCD, prioritate 1. |
| 438 | `xTaskCreate(TaskLEDStatus, ..., + 1, ...)` | Task LED status, prioritate 1. |
| 440 | `vTaskStartScheduler();` | Pornește kernel FreeRTOS; nu revine dacă totul e OK. |
| 441 | `return 0;` | Teoretic inaccesibil dacă scheduler-ul rulează. |

---

### `initPLL`

| Linie | Cod | Explicație |
|-------|-----|------------|
| 447 | `static void initPLL(void)` | Configurează oscilatorul intern + PLL pentru Fcy ≈ 40 MHz. |
| 449 | `PLLFBD = 41;` | Multiplicator PLL (N) — parte din formula frecvență. |
| 450 | `CLKDIVbits.PLLPOST = 0;` | Divizor post-PLL /2 (bit 0 → divide by 2). |
| 451 | `CLKDIVbits.PLLPRE = 0;` | Prescaler intrare PLL. |
| 452 | `__builtin_write_OSCCONH(0x01);` | Selectează sursa ceas: PLL (scris securizat registru OSCCON). |
| 453 | `__builtin_write_OSCCONL(0x01);` | Pornește comutarea la noul oscilator. |
| 454 | `while (OSCCONbits.COSC != 0b001);` | Așteaptă ca ceasul activ să fie PLL. |
| 455 | `while (OSCCONbits.LOCK != 1);` | Așteaptă blocarea PLL înainte de periferice sensibile la timp. |

---

### `initPWM`

| Linie | Cod | Explicație |
|-------|-----|------------|
| 459 | `static void initPWM(void)` | PWM1 modul 3 pe RB10 pentru servo. |
| 461 | `P1FLTACON = 0;` | Dezactivează filtre fault PWM. |
| 462 | `P1OVDCON = 0x3F00;` | Configurare override ieșiri PWM. |
| 464-465 | `P1TCON = 0; P1TMR = 0;` | Resetează control și contor timer PWM. |
| 467 | `P1TCONbits.PTCKPS = 3;` | Prescaler ceas timer: împărțire la 64. |
| 468 | `P1TCONbits.PTMOD = 0;` | Mod free-running (perioadă fixă P1TPER). |
| 470 | `P1TPER = PWM_PERIOD;` | Perioadă ~20 ms (12500 tick-uri la Fcy/64). |
| 471 | `P1DC3 = PWM_CENTER;` | Duty inițial centru (~1,5 ms). |
| 473 | `PWM1CON1 = 0;` | Reset configurare canal PWM. |
| 474 | `PWM1CON1bits.PMOD3 = 1;` | Mod PWM independent canal 3. |
| 475 | `PWM1CON1bits.PEN3H = 1;` | Activează ieșirea high PWM3 (RB10). |
| 476 | `PWM1CON1bits.PEN3L = 0;` | Dezactivează ieșirea low complementară. |
| 478 | `PWM1CON2 = 0;` | Reset CON2. |
| 479 | `PWM1CON2bits.IUE = 1;` | Immediate update enable pentru duty. |
| 481 | `_TRISB10 = 0;` | RB10 ca ieșire. |
| 482 | `_LATB10 = 0;` | Nivel latch inițial low. |
| 484 | `P1TCONbits.PTEN = 1;` | Pornește timerul PWM. |

---

### `initAdc`

| Linie | Cod | Explicație |
|-------|-----|------------|
| 488 | `static void initAdc(void)` | ADC1 pentru potențiometru AN5 (RB3). |
| 490 | `AD1CON1bits.AD12B = 1;` | Mod 12 biți. |
| 491 | `AD1CON1bits.SSRC = 2;` | Start conversie: Timer3 overflow. |
| 492 | `AD1CON1bits.ASAM = 1;` | Auto-sample activ. |
| 493 | `AD1CON2bits.CSCNA = 1;` | Scanare canale (folosește lista CSS). |
| 494 | `AD1CON3bits.ADRC = 0;` | Ceas ADC derivat din SYSCLK. |
| 495 | `AD1CON3bits.ADCS = 63;` | Timp eșantionare Tad (maxim în registrul ADCS). |
| 497 | `AD1CSSLbits.CSS5 = 1;` | Include AN5 în scanare. |
| 498 | `AD1PCFGL = 0xFFFF;` | Toate pinurile analogice dezactivate implicit. |
| 499 | `AD1PCFGLbits.PCFG5 = 0;` | AN5/RB3 configurat analogic. |
| 500 | `TRISBbits.TRISB3 = 1;` | RB3 intrare (potențiometru). |
| 502 | `_AD1IF = 0;` | Curăță flag întrerupere ADC. |
| 503 | `_AD1IE = 1;` | Activează întreruperea ADC1. |
| 504 | `AD1CON1bits.ADON = 1;` | Pornește modulul ADC. |

---

### `initTimer3`

| Linie | Cod | Explicație |
|-------|-----|------------|
| 507 | `static void initTimer3(void)` | Timer 3 declanșează eșantionări ADC. |
| 509 | `TMR3 = 0;` | Resetează contorul. |
| 510 | `PR3 = 50000;` | Perioadă overflow — definește rata de citire ADC. |
| 511 | `T3CONbits.TON = 1;` | Pornește Timer3. |

---

### `prvSetupHardware`

| Linie | Cod | Explicație |
|-------|-----|------------|
| 514 | `static void prvSetupHardware(void)` | Inițializare centralizată hardware. |
| 516 | `ADPCFG = 0xFFFF;` | Implicit toate pinurile digitali (apoi ADC activează AN5). |
| 517 | `PORTB = 0x0000;` | Niveluri PORTB inițiale 0. |
| 518 | `TRISB = 0x0000;` | Implicit toate ieșiri; pinii speciali se reconfigurează. |
| 520 | `initPLL();` | Ceas sistem ~40 MHz. |
| 522-523 | `TRIS_LED_* = 0` | LED RB11 și RB1 ca ieșiri. |
| 525-526 | `LED_STATUS = 0; LED_MODE = LED_MODE_AUTO_PIN;` | LED-uri active-low aprinse la start (mod AUTO). |
| 528 | `_TRISB7 = 1;` | RB7 intrare pentru buton INT0. |
| 529-532 | `_INT0IF=0; _INT0IE=1; _INT0EP=1; _INT0IP=6` | Activează INT0, front descrescător, prioritate 6. |
| 534 | `CNPU1 = 0x0040;` | Pull-up pe RB7 (bit 6). |
| 535 | `output_float();` | Pin 1-Wire RB2 în high impedanță. |
| 536 | `ONE_WIRE_PIN = 1;` | Linie 1-Wire la nivel high înainte de trafic. |
| 538-540 | `initPWM(); initAdc(); initTimer3();` | Periferice servo și potențiometru. |
| 542-546 | `LCD_init();` + mesaje implicite | Pornește LCD și afișează text inițial pe 4 linii. |
| 548 | `xSerialPortInitMinimal(9600, comBUFFER_LEN);` | UART1 la 9600 baud, cozi FreeRTOS pentru RX/TX. |

---

### `setServoDC`

| Linie | Cod | Explicație |
|-------|-----|------------|
| 551 | `static void setServoDC(unsigned int dc)` | Setează lățimea pulsului servo. |
| 553 | `P1DC3 = dc;` | Scrie duty-cycle în registrul canalului 3 PWM. |

---

### `vApplicationIdleHook`

| Linie | Cod | Explicație |
|-------|-----|------------|
| 556 | `void vApplicationIdleHook(void)` | Hook FreeRTOS apelat în task-ul idle. |
| 557 | `}` | Corp gol — nu face nimic suplimentar la idle. |

---

## `new_lcd.c` — driver LCD

### `delayus`

| Linie | Cod | Explicație |
|-------|-----|------------|
| 15 | `void delayus(int us)` | Întârziere busy-wait în microsecunde. |
| 16 | `int i;` | Contor buclă. |
| 17 | `for(i=0;i<us;i++)` | Repetă `us` ori un microsecund echivalent. |
| 19-26 | 32× `asm("nop");` | NOP-uri inline — calibrează timpul la Fcy curent (~1 µs per iterație). |

---

### `delayms`

| Linie | Cod | Explicație |
|-------|-----|------------|
| 30 | `void delayms(int ms)` | Întârziere în milisecunde. |
| 32 | `int i;` | Contor. |
| 33 | `for(i=0;i<ms;i++) delayus(1000);` | 1000 µs × ms = ms milisecunde. |

---

### `LCD_DATA_OR` / `LCD_DATA_AND`

| Linie | Cod | Explicație |
|-------|-----|------------|
| 38 | `void LCD_DATA_OR(int val)` | Pune biții pe liniile de date LCD (RB15–RB12). |
| 40 | `LATB \|= (val << 12);` | OR pe LATB, nu PORTB — evită citirea RB10 (PWM) la RMW. |
| 43 | `void LCD_DATA_AND(int val)` | Șterge biți selectivi pe liniile de date. |
| 45 | `LATB &= ((val << 12) \| 0xFFFu);` | Maskează doar nibbles D7–D4; păstrează restul pinilor PORTB. |

---

### `clear`

| Linie | Cod | Explicație |
|-------|-----|------------|
| 49 | `void clear(void)` | Comandă LCD „clear display”. |
| 51-53 | nibble 0x0 apoi strobe E | Pregătire nibble superior. |
| 55-57 | nibble 0x01 + `delayms(2)` | Comandă clear; timp execuție ~2 ms pe HD44780. |

---

### `send_char2LCD`

| Linie | Cod | Explicație |
|-------|-----|------------|
| 62 | `void send_char2LCD(char ax)` | Trimite un caracter în mod 4 biți. |
| 63-66 | nibble superior + `LCD_RS=1` | RS=1 → date (nu comandă); strobe E. |
| 67-70 | nibble inferior + strobe | Al doilea nibble + RS=0 la final. |
| 71 | `delayus(40)` | Timp setup după caracter. |

---

### `LCD_line`

| Linie | Cod | Explicație |
|-------|-----|------------|
| 75 | `void LCD_line(int linie)` | Setează DDRAM la începutul liniei (1–4). |
| 77 | `char ax=0;` | Adresă DDRAM. |
| 78-84 | `switch` | L1=0x80, L2=0xC0, L3=0x94, L4=0xD4 (controller 4×20). |
| 85-90 | două strobes E | Trimite adresa în 4-bit mode. |

---

### `LCD_init`

| Linie | Cod | Explicație |
|-------|-----|------------|
| 94 | `void LCD_init(void)` | Secvență power-on HD44780 4-bit. |
| 97 | `LCD_ON = 1;` | Activează alimentarea/backlight (pin definit RB7 în acest fișier). |
| 98 | `delayms(40);` | Timp stabilizare după power-on. |
| 101-107 | Function set 0x02 de 3 ori | Wake-up 8-bit → trecere 4-bit. |
| 109-111 | `0x08` pe nibbles | Function set: 4-bit, 2 linii, font 5×8. |
| 114-120 | `0x00` apoi `0x0E` | Display on, cursor on, blink off. |
| 123 | `clear();` | Șterge ecranul. |
| 125-131 | `0x06` | Entry mode: increment cursor, no shift. |
| 134 | `delayms(2);` | Pauză finală inițializare. |

---

### `LCD_printf`

| Linie | Cod | Explicație |
|-------|-----|------------|
| 138 | `void LCD_printf(char *text)` | Afișează șir la poziția curentă. |
| 140 | `int i;` | Index. |
| 141 | `for (i=0;i<strlen(text);i++) send_char2LCD(...)` | Trimite fiecare caracter. |

---

### `LCD_Goto`

| Linie | Cod | Explicație |
|-------|-----|------------|
| 145 | `void LCD_Goto(int linie, int col)` | Poziționare linie + coloană (1-based). |
| 147-154 | `switch` + adrese | Aceleași baze DDRAM ca `LCD_line`. |
| 155 | `ax = ax + col - 1;` | Offset coloană. |
| 156-161 | strobes E | Trimite adresa completă. |

---

### `LCD_On_Off`

| Linie | Cod | Explicație |
|-------|-----|------------|
| 165 | `void LCD_On_Off(int On_Off)` | Comandă display on/off. |
| 167 | `char ax = 0;` | Byte comandă. |
| 168-172 | `switch` | On=0x0A, Off=0x08 (variante control display). |
| 173-176 | strobe + delay | Trimite și așteaptă. |

---

## `new_serial.c` — UART1 + FreeRTOS

### `xSerialPortInitMinimal`

| Linie | Cod | Explicație |
|-------|-----|------------|
| 45 | `xComPortHandle xSerialPortInitMinimal(...)` | Inițializează UART minimal pentru demo FreeRTOS. |
| 47 | `char cChar;` | Temporar pentru golire buffer RX. |
| 49 | `TRISBbits.TRISB9 = 1;` | RB9 intrare (RX). |
| 51 | `PORTBbits.RB8 = 1;` | RB8 idle high înainte de mapare. |
| 54 | `RPOR4 = 3;` | Remap U1TX → RP4 → pin RB8. |
| 55 | `RPINR18 = 9;` | Remap U1RX de la pin RB9. |
| 58-59 | `xQueueCreate` ×2 | Cozi FreeRTOS: caractere primite / de transmis. |
| 62-71 | `U1MODEbits.*` | 8N1, fără auto-baud, UART activ. |
| 73 | `U1BRG = ... configCPU_CLOCK_HZ / (16 * baud) - 0.5` | Calculează registrul baud pentru BRG low speed. |
| 75-79 | `U1STAbits.*` | RX/TX pe caracter, transmisie activată. |
| 84 | `portDISABLE_INTERRUPTS();` | Blochează IRQ înainte de configurare (înainte de scheduler). |
| 86-91 | curăță flag-uri, setează prioritate, activează IE | Pregătește ISR-uri U1RX/U1TX la prioritate kernel. |
| 94-97 | `while (URXDA) U1RXREG` | Golește FIFO RX la init. |
| 99 | `xTxHasEnded = pdTRUE;` | Stare: transmițătorul e liber. |
| 101 | `return NULL;` | Un singur port — handle nefolosit. |

---

### `xSerialGetChar`

| Linie | Cod | Explicație |
|-------|-----|------------|
| 105 | `signed portBASE_TYPE xSerialGetChar(...)` | Citește un caracter din coada RX. |
| 108 | `(void) pxPort;` | Port unic, parametru ignorat. |
| 112 | `if (xQueueReceive(xRxedChars, pcRxedChar, xBlockTime))` | Așteaptă caracter din ISR până la timeout. |
| 114 | `return pdTRUE;` | Succes. |
| 118 | `return pdFALSE;` | Timeout / coadă goală. |

---

### `xSerialPutChar`

| Linie | Cod | Explicație |
|-------|-----|------------|
| 123 | `signed portBASE_TYPE xSerialPutChar(...)` | Pune un caracter în coada TX. |
| 129 | `if (xQueueSend(...) != pdPASS) return pdFAIL;` | Eșec dacă coada e plină în timpul block time. |
| 136 | `if (xTxHasEnded)` | Dacă transmisia anterioară s-a terminat. |
| 138-139 | `xTxHasEnded = pdFALSE; IFS0bits.U1TXIF = 1;` | Pornește ISR TX forțând flag-ul. |
| 142 | `return pdPASS;` | Caracter acceptat în coadă. |

---

### `vSerialClose`

| Linie | Cod | Explicație |
|-------|-----|------------|
| 146 | `void vSerialClose(xComPortHandle xPort)` | Închidere port — neimplementată (gol). |

---

### `_U1RXInterrupt`

| Linie | Cod | Explicație |
|-------|-----|------------|
| 151 | `void __attribute__((__interrupt__, auto_psv)) _U1RXInterrupt(void)` | ISR primire UART1. |
| 154 | `portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;` | Flag pentru context switch din ISR. |
| 159 | `IFS0bits.U1RXIF = 0;` | Curăță flag RX. |
| 160 | `while (U1STAbits.URXDA)` | Procesează toate caracterele disponibile. |
| 162-163 | `cChar = U1RXREG; xQueueSendFromISR(...)` | Citește registrul și postează în coadă. |
| 166-169 | `if (xHigherPriorityTaskWoken) taskYIELD();` | Schimbă context dacă un task mai prioritar așteaptă. |

---

### `_U1TXInterrupt`

| Linie | Cod | Explicație |
|-------|-----|------------|
| 173 | `void __attribute__((__interrupt__, auto_psv)) _U1TXInterrupt(void)` | ISR transmitere UART1. |
| 176 | `portBASE_TYPE xTaskWoken = pdFALSE;` | Idem pentru yield. |
| 181 | `IFS0bits.U1TXIF = 0;` | Curăță flag TX. |
| 182 | `while (!(U1STAbits.UTXBF))` | Cât timp bufferul TX nu e plin. |
| 184-187 | `xQueueReceiveFromISR` + `U1TXREG = cChar` | Trimite următorul caracter din coadă. |
| 189-193 | `else { xTxHasEnded = pdTRUE; break; }` | Coadă goală → oprește lanțul TX. |
| 197-200 | `taskYIELD` condiționat | Wake task-uri blocate pe TX. |

---

### `vSerialPutString`

| Linie | Cod | Explicație |
|-------|-----|------------|
| 204 | `void vSerialPutString(...)` | Trimite un șir terminat cu `\0`. |
| 209-210 | `(void) usStringLength; (void) pxPort;` | Parametri nefolosiți. |
| 218 | `pxNext = (signed char *) pcString;` | Pointer la primul caracter. |
| 219 | `while (*pxNext)` | Parcurge șirul. |
| 222 | `xSerialPutChar(pxPort, *pxNext, 0);` | Pune fiecare caracter în coadă fără block. |
| 223 | `pxNext++;` | Următorul caracter. |

---

## `ds18b20.c` — senzor 1-Wire

### `output_low` / `output_float` / `output_bit` / `input`

| Linie | Cod | Explicație |
|-------|-----|------------|
| 5 | `void output_low()` | Pin RB2 ca ieșire, nivel 0 — trage bus-ul jos. |
| 5 | `ONE_WIRE_DIR = 0` | Direcție ieșire. |
| 5 | `ONE_WIRE_PIN = 0` | Nivel low. |
| 7 | `void output_float()` | Pin ca intrare (high-Z); bus eliberat, pull-up extern ridică linia. |
| 7 | `ONE_WIRE_DIR = 1` | Input. |
| 9 | `void output_bit(int val)` | Scrie un bit pe bus (ieșire + nivel val). |
| 11 | `int input()` | Citește nivelul logic al bus-ului 1-Wire. |
| 11 | `return ONE_WIRE_PIN` | Valoare 0 sau 1. |

---

### `onewire_reset`

| Linie | Cod | Explicație |
|-------|-----|------------|
| 16 | `void onewire_reset()` | Impuls reset 1-Wire (~480 µs low + recovery). |
| 18 | `output_float();` | Eliberează bus high. |
| 19 | `output_low();` | Trage bus low. |
| 20 | `delayus(550);` | Puls reset > 480 µs. |
| 21 | `output_float();` | Eliberează pentru presence pulse slave. |
| 22 | `delayus(70);` | Fereastră presence detect (fără verificare explicită). |
| 24 | `delayus(500);` | Așteaptă sfârșitul ferestrei de reset. |
| 25 | `output_float();` | Bus gata pentru comenzi. |

---

### `onewire_write`

| Linie | Cod | Explicație |
|-------|-----|------------|
| 31 | `void onewire_write(int data)` | Scrie un octet LSB-first pe bus. |
| 33 | `int count;` | Contor biți 0..7. |
| 34 | `for (count=0; count<8; ++count)` | 8 time-slot-uri scriere. |
| 36 | `output_low();` | Începe slot scriere. |
| 37 | `delayus(2);` | Timp minim low. |
| 39 | `output_bit(data&0x01);` | Scrie bitul curent (cel mai puțin semnificativ). |
| 40 | `data = data >> 1;` | Shift pentru următorul bit. |
| 42 | `delayus(60);` | Menține slot conform timp DS18B20. |
| 43 | `output_float();` | Eliberează bus. |
| 44 | `delayus(2);` | Recovery între biți. |

---

### `onewire_read`

| Linie | Cod | Explicație |
|-------|-----|------------|
| 51 | `short int onewire_read()` | Citește un octet de pe bus. |
| 53 | `short int count, data;` | Contor și acumulator. |
| 55 | `data = 0;` | Inițializare. |
| 56 | `for (count=0; count<8; ++count)` | 8 slot-uri citire. |
| 58 | `output_low();` | Master trage bus low. |
| 59 | `delayus(2);` | Timp inițiere read slot. |
| 60 | `output_float();` | Eliberează; slave pune bitul. |
| 61 | `delayus(8);` | Timp stabilizare înainte de eșantionare. |
| 63 | `data = data >> 1;` | Shift dreapta pentru noul bit. |
| 64 | `data = data \| (input()<<7);` | Citește bit în poziția MSB a octetului în construcție. |
| 66 | `delayus(120);` | Așteaptă sfârșitul slot-ului de citire. |
| 68 | `return data;` | Returnează octetul montat. |

---

### `ds1820_read`

| Linie | Cod | Explicație |
|-------|-----|------------|
| 71 | `float ds1820_read()` | Citește temperatura în °C de la DS18B20. |
| 73-74 | variabile `busy`, `temp_LSB/MSB`, `byte2..7` | Stare conversie și scratchpad 9 octeți. |
| 75-76 | `cod_temp`, `temp`, `temp_prec` | Rezultat brut și float (precizie extinsă nefolosită la return). |
| 78 | `onewire_reset();` | Reset bus înainte de comandă. |
| 79 | `onewire_write(0xCC);` | Skip ROM — un singur senzor pe bus. |
| 80 | `onewire_write(0x44);` | Convert T — pornește conversia temperaturii. |
| 82-83 | `while (busy == 0) busy = onewire_read();` | Așteaptă bit busy din scratchpad (conversie terminată). |
| 85 | `onewire_reset();` | Reset pentru citire. |
| 86-87 | `0xCC`, `0xBE` | Skip ROM + Read Scratchpad. |
| 88-95 | `onewire_read()` ×9 | Citește LSB, MSB și restul octeților scratchpad. |
| 97 | `cod_temp = (temp_MSB<<8) + temp_LSB;` | Temperatura brută 16-bit semnată. |
| 99 | `temp = (float) cod_temp / 16.0;` | DS18B20: rezoluție 1/16 °C per LSB. |
| 100 | `temp_prec = ...` | Calcul alternativ (comentat la return). |
| 102 | `return(temp);` | Returnează temperatura în float. |

---

## Note

- **`delayus` / `delayms`**: definite în `new_lcd.c`, declarate în `new_lcd.h`; folosite și de `ds18b20.c` la linkare.
- **Conflict potențial pini**: în `new_lcd.c`, `LCD_ON` este `_RB7`; în `main.c`, RB7 este **INT0** (buton). La rulare, verifică schema plăcii — poate fi macro neactualizat sau alt hardware.
- **Regenerare PDF/DOCX** din acest fișier: `python scripts/readme_to_doc.py` (adaptând scriptul să citească `README_FUNCTII.md` dacă dorești export).

---

*Proiect PATR — documentație funcții*
