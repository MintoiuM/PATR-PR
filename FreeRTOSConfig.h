/*
    FreeRTOS V6.0.4 - Copyright (C) 2010 Real Time Engineers Ltd.
*/

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include "p33Fxxxx.h"

/*-----------------------------------------------------------
 * Application specific definitions.
 *
 * These definitions should be adjusted for your particular hardware and
 * application requirements.
 *
 *----------------------------------------------------------*/

#define configUSE_PREEMPTION			1	// utilizarea planificarii preemtive sau cooperative
#define configUSE_IDLE_HOOK				1	// utilizarea unei fct "hook" pentru tak-ul idle
#define configUSE_TICK_HOOK				0	// utilizarea unei fct "hook" pentru "tick-ul" sistemului
#define configTICK_RATE_HZ				( ( portTickType ) 1000 )	// frecventa "tick-ului" sistemului
#define configCPU_CLOCK_HZ				( ( unsigned long ) 40000000 )  /* Fosc / 2 */	// ceasul magistralei
//#define configCPU_CLOCK_HZ				( ( unsigned long ) 3685000 )  /* Fosc / 2 */
#define configMAX_PRIORITIES			( ( unsigned portBASE_TYPE ) 4 )	// numarul de prioritati pt. task-uri
#define configMINIMAL_STACK_SIZE		( 105 )				// dimensiunea stivei task-ului idle
#define configTOTAL_HEAP_SIZE			( ( size_t ) 5120 )	// dimensiunea memoriei RAM dedicate elementelor sist.
#define configMAX_TASK_NAME_LEN			( 4 )				// numarul maxim de caractere ale numelui task-urilor
#define configUSE_TRACE_FACILITY		0	// indica utilizarea sau nu a facilitatii de TRACE
#define configUSE_16_BIT_TICKS			1	// indica folosirea unui timer de 16 sau 32 de biti pentru numararea ticks
#define configIDLE_SHOULD_YIELD			1	// daca task-urile cu prioritate idle (0) sa se intrerupa cind exista task-uri prioritare

/* Co-routine definitions. */
#define configUSE_CO_ROUTINES 		1			// indica utilizarea sau nu a corutinelor
#define configMAX_CO_ROUTINE_PRIORITIES ( 2 )	//

/* Set the following definitions to 1 to include the API function, or zero
to exclude the API function. */

#define INCLUDE_vTaskPrioritySet		1
#define INCLUDE_uxTaskPriorityGet		0
#define INCLUDE_vTaskDelete				0
#define INCLUDE_vTaskCleanUpResources	0
#define INCLUDE_vTaskSuspend			1
#define INCLUDE_vTaskDelayUntil			1
#define INCLUDE_vTaskDelay				1


#define configKERNEL_INTERRUPT_PRIORITY	0x01

#endif /* FREERTOS_CONFIG_H */
