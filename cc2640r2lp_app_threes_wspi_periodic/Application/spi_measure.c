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

#include <ti/drivers/SPI.h>
#include <ti/drivers/GPIO.h>
#include "Board.h"

#include <uartlog/UartLog.h>
#include <xdc/runtime/Diags.h>

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

#define SPI_MSG_LENGTH  (2)
#define MAX_LOOP        (14)

Task_Struct SPITask;
Char SPITaskStack[SPI_TASK_STACK_SIZE];

unsigned char masterTxBuffer[SPI_MSG_LENGTH];
unsigned char masterRxBuffer[SPI_MSG_LENGTH];

static void SPI_taskFxn(UArg a0, UArg a1);

int16_t To16int(char H_V, char L_V){
    uint16_t uvalue16 = (H_V << 8)|(L_V);
    int16_t result;
    result = (int16_t) uvalue16;
    return result;
}

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
    SPI_Handle      masterSpi;
    SPI_Params      spiParams;
    SPI_Transaction transaction;
    uint32_t        i;
    bool            transferOK;
    //int32_t         status;
    //GPIO_setConfig(Board_SPI_MASTER_READY, GPIO_CFG_OUTPUT | GPIO_CFG_OUT_LOW);
    //GPIO_setConfig(Board_SPI_SLAVE_READY, GPIO_CFG_INPUT);
    //GPIO_write(Board_SPI_MASTER_READY, 1);
    //while (GPIO_read(Board_SPI_SLAVE_READY) == 0) {}
    /* Handshake complete; now configure interrupt on Board_SPI_SLAVE_READY */
    //GPIO_setConfig(Board_SPI_SLAVE_READY, GPIO_CFG_IN_PU | GPIO_CFG_IN_INT_FALLING);

    SPI_Params_init(&spiParams);
    spiParams.frameFormat = SPI_POL1_PHA1;
    spiParams.bitRate = 1000000;
    masterSpi = SPI_open(Board_SPI_MASTER, &spiParams);
    if (masterSpi == NULL) {
        while (1);
    }
    //GPIO_write(Board_SPI_MASTER_READY, 0);
    char initial_reg = 0xBB;
    char msg_tx[] = {initial_reg, 0x00};
    int16_t meas_values[14];
    while (1){
        for (i = 0; i < MAX_LOOP; i++) {
            msg_tx[0] = initial_reg+i;
            strncpy((char *) masterTxBuffer, msg_tx, SPI_MSG_LENGTH);
            memset((void *) masterRxBuffer, 0, SPI_MSG_LENGTH);
            Log_info2("Master sent: %u, %u", masterTxBuffer[1], masterTxBuffer[0]);
            transaction.count = SPI_MSG_LENGTH;
            transaction.txBuf = (void *) masterTxBuffer;
            transaction.rxBuf = (void *) masterRxBuffer;
            transferOK = SPI_transfer(masterSpi, &transaction);
            if (transferOK){
                Log_info2("Master received: %u, %u", masterRxBuffer[1], masterRxBuffer[0]);
                meas_values[i] = masterRxBuffer[1];
            }
        }
        myAccx = To16int(meas_values[0], meas_values[1]);
        myAccy = To16int(meas_values[2], meas_values[3]);
        myAccz = To16int(meas_values[4], meas_values[5]);
        myTemp = To16int(meas_values[6], meas_values[7]);
        myGyrox = To16int(meas_values[8], meas_values[9]);
        myGyroy = To16int(meas_values[10], meas_values[11]);
        myGyroz = To16int(meas_values[12], meas_values[13]);
        MyAcc_SetParameter(MYACC_MYACCX_ID, MYACC_MYACCX_LEN, &myAccx);
        MyAcc_SetParameter(MYACC_MYACCY_ID, MYACC_MYACCY_LEN, &myAccy);
        MyAcc_SetParameter(MYACC_MYACCZ_ID, MYACC_MYACCZ_LEN, &myAccz);
        MyTemp_SetParameter(MYTEMP_MYT_ID, MYTEMP_MYT_LEN, &myTemp);
        MyGyro_SetParameter(MYGYRO_MYGYROX_ID, MYGYRO_MYGYROX_LEN, &myGyrox);
        MyGyro_SetParameter(MYGYRO_MYGYROY_ID, MYGYRO_MYGYROY_LEN, &myGyroy);
        MyGyro_SetParameter(MYGYRO_MYGYROZ_ID, MYGYRO_MYGYROZ_LEN, &myGyroz);
        Log_info3("myAccx is %d, myAccy is %d and myAccz is %d", (IArg)myAccx,
                  (IArg)myAccy, (IArg)myAccz);
        Log_info3("myGyrox is %d, myGyroy is %d and myGyroz is %d", (IArg)myGyrox,
                  (IArg)myGyroy, (IArg)myGyroz);
        Log_info1("myTemp is %d", (IArg)myTemp);
        Task_sleep(1000000/Clock_tickPeriod);
    }
}
