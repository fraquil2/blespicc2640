/*******************************************************************************

* INCLUDES
 */
#include <stdint.h>
#include <string.h>
#include <project_zero.h>
#include <spi_measure.h>

//#define xdc_runtime_Log_DISABLE_ALL 1  // Add to disable logs from this file

#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Event.h>
#include <ti/sysbios/knl/Queue.h>
#include <ti/sysbios/knl/Clock.h>

#include "icall_ble_api.h"
#include "myAcc.h"
#include "myTemp.h"
#include "myGyro.h"

//DEFINES
// Task configuration
#define SPI_TASK_PRIORITY                     1

#ifndef SPI_TASK_STACK_SIZE
#define SPI_TASK_STACK_SIZE                   1000
#endif
/*
extern uint8_t MYACC_MYACCX_ID;
extern uint8_t MYACC_MYACCY_ID;
extern uint8_t MYACC_MYACCZ_ID;
extern uint8_t MYTEMP_MYT_ID;
extern uint8_t MYGYRO_MYGYROX_ID;
extern uint8_t MYGYRO_MYGYROY_ID;
extern uint8_t MYGYRO_MYGYROZ_ID;

extern uint16_t MYACC_MYACCX_LEN;
extern uint16_t MYACC_MYACCY_LEN;
extern uint16_t MYACC_MYACCZ_LEN;
extern uint16_t MYTEMP_MYT_LEN;
extern uint16_t MYGYRO_MYGYROX_LEN;
extern uint16_t MYGYRO_MYGYROY_LEN;
extern uint16_t MYGYRO_MYGYROZ_LEN;

extern bStatus_t MyTemp_SetParameter( uint8_t param, uint16_t len, void *value );
extern bStatus_t MyAcc_SetParameter( uint8_t param, uint16_t len, void *value );
extern bStatus_t MyGyro_SetParameter( uint8_t param, uint16_t len, void *value );
*/

static uint32_t myAccx = 0;
static uint32_t myAccy = 0;
static uint32_t myAccz = 0;
static uint32_t myTemp = 0;
static uint32_t myGyrox = 0;
static uint32_t myGyroy = 0;
static uint32_t myGyroz = 0;

Task_Struct SPITask;
Char SPITaskStack[SPI_TASK_STACK_SIZE];

static void SPI_taskFxn(UArg a0, UArg a1);

void SPI_createTask(void)
{
  Task_Params taskParams;

  // Configure task
  Task_Params_init(&taskParams);
  taskParams.stack = SPITaskStack;
  taskParams.stackSize = SPI_TASK_STACK_SIZE;
  taskParams.priority = SPI_TASK_PRIORITY;

  Task_construct(&SPITask, SPI_taskFxn, &taskParams, NULL);
}


static void SPI_taskFxn(UArg a0, UArg a1){

    while (1){
        myAccx += 1;
        myAccy += 2;
        myAccz += 3;
        myTemp += 4;
        myGyrox += 1;
        myGyroy += 2;
        myGyroz += 3;
        /*MyAcc_SetParameter(MYACC_MYACCX_ID,
                           MYACC_MYACCX_LEN,
                           &myAccx);*/
        /*MyAcc_SetParameter(MYACC_MYACCY_ID,
                           MYACC_MYACCY_LEN,
                           &myAccy);
        MyAcc_SetParameter(MYACC_MYACCZ_ID,
                           MYACC_MYACCZ_LEN,
                           &myAccz);
        MyTemp_SetParameter(MYTEMP_MYT_ID,
                            MYTEMP_MYT_LEN,
                            &myTemp);
        MyGyro_SetParameter(MYGYRO_MYGYROX_ID,
                            MYGYRO_MYGYROX_LEN,
                            &myGyrox);
        MyGyro_SetParameter(MYGYRO_MYGYROY_ID,
                            MYGYRO_MYGYROY_LEN,
                            &myGyroy);
        MyGyro_SetParameter(MYGYRO_MYGYROZ_ID,
                            MYGYRO_MYGYROZ_LEN,
                            &myGyroz);*/
        Task_sleep(100000);
    }
}
