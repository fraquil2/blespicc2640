/*
 * Copyright (c) 2016, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,

 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  ======== main_tirtos.c ========
 */
/* Standard Includes */
#include <stdint.h>
#include <pthread.h>

/* TI Includes */
#include <ti/sysbios/BIOS.h>
#include <ti/drivers/GPIO.h>
#include <ti/drivers/SPI.h>
#include <ti/drivers/UART.h>
#include <ti/drivers/Timer.h>
#include <ti/drivers/net/wifi/simplelink.h>
#include <ti/display/Display.h>
#include <semaphore.h>

/* Local Files */
#include "ap_provisioning.h"
#include "ble_provisioning.h"
#include "Board.h"

/* Main Application Threads */
extern void *bleThread(void *arg0);
extern void *wifiThread(void *arg0);
extern void *mainThread(void *arg0);
extern void *apThread(void *arg);

/* Stack size in bytes */
#define THREAD_STACK_SIZE    4096

/* Output display handle that will be used to print out all debug/log
 * statements
 */
Display_Handle displayOut;

/* Control switching between provisioning modes */
extern sem_t AP_connectionAsyncSem;

/*
 *  ======== main ========
 */
int main(void)
{
    pthread_t thread;
    pthread_attr_t pAttrs;
    struct sched_param priParam;
    int retc;
    int detachState;

    /* Call board init functions */
    Board_initGeneral();
    GPIO_init();
    UART_init();
    SPI_init();
    Timer_init();

    /* Switch off all LEDs on boards */
    GPIO_write(Board_LED0, Board_LED_OFF);
    GPIO_write(Board_LED1, Board_LED_OFF);
    GPIO_write(Board_LED2, Board_LED_OFF);

    /* Open the display for output */
    displayOut = Display_open(Display_Type_UART | Display_Type_HOST, NULL);
    if (displayOut == NULL)
    {
        /* Failed to open display driver */
        while (1);
    }

    /* Set priority and stack size attributes */
    pthread_attr_init(&pAttrs);
    priParam.sched_priority = 1;
    detachState = PTHREAD_CREATE_DETACHED;
    retc = pthread_attr_setdetachstate(&pAttrs, detachState);
    if (retc != 0)
    {
        /* pthread_attr_setdetachstate() failed */
        while (1);
    }

    /* Initialize semaphores */
    retc = sem_init(&AP_connectionAsyncSem, 0, 0);
    if (retc == -1)
    {
        while (1)
            ;
    }

    pthread_attr_init(&pAttrs);
    priParam.sched_priority = 3;
    retc = pthread_attr_setschedparam(&pAttrs, &priParam);
    retc |= pthread_attr_setstacksize(&pAttrs, TASK_STACK_SIZE);

    if(retc)
    {
        /* error handling */
        while(1);
    }

    /* Create AP thread */
    retc = pthread_create(&thread, &pAttrs, apThread, NULL);

    priParam.sched_priority = 1;
    pthread_attr_setschedparam(&pAttrs, &priParam);

    retc |= pthread_attr_setstacksize(&pAttrs, THREAD_STACK_SIZE);
    if (retc != 0)
    {
        /* pthread_attr_setstacksize() failed */
        while (1);
    }

    /* Create BLE thread */
    retc = pthread_create(&thread, &pAttrs, bleThread, NULL);
    if (retc != 0)
    {
        /* pthread_create() failed */
        while (1);
    }

    priParam.sched_priority = 2;
    pthread_attr_setschedparam(&pAttrs, &priParam);

    /* Create WiFi thread */
    retc = pthread_create(&thread, &pAttrs, wifiThread, NULL);
    if (retc != 0)
    {
        /* pthread_create() failed */
        while (1);
    }

    BIOS_start();

    return (0);
}

/*
 *  ======== dummyOutput ========
 *  Dummy SysMin output function needed for benchmarks and size comparison
 *  of FreeRTOS and TI-RTOS solutions.
 */
void dummyOutput(void)
{

}
