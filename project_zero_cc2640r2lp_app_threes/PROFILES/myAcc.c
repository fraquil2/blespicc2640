/**********************************************************************************************
 * Filename:       myAcc.c
 *
 * Description:    This file contains the implementation of the service.
 *
 * Copyright (c) 2015-2020, Texas Instruments Incorporated
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
 *
 *************************************************************************************************/


/*********************************************************************
 * INCLUDES
 */
#include <string.h>

#include <icall.h>

/* This Header file contains all BLE API and icall structure definition */
#include "icall_ble_api.h"

#include "myAcc.h"
#include <uartlog/UartLog.h>

/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * CONSTANTS
 */

/*********************************************************************
 * TYPEDEFS
 */

/*********************************************************************
* GLOBAL VARIABLES
*/

// myAcc Service UUID
CONST uint8_t myAccUUID[ATT_UUID_SIZE] =
{
  TI_BASE_UUID_128(MYACC_SERV_UUID)
};

// myAccX UUID
CONST uint8_t myAcc_MyAccXUUID[ATT_UUID_SIZE] =
{
  TI_BASE_UUID_128(MYACC_MYACCX_UUID)
};
// myAccY UUID
CONST uint8_t myAcc_MyAccYUUID[ATT_UUID_SIZE] =
{
  TI_BASE_UUID_128(MYACC_MYACCY_UUID)
};
// myAccZ UUID
CONST uint8_t myAcc_MyAccZUUID[ATT_UUID_SIZE] =
{
  TI_BASE_UUID_128(MYACC_MYACCZ_UUID)
};

/*********************************************************************
 * LOCAL VARIABLES
 */

static myAccCBs_t *pAppCBs = NULL;

/*********************************************************************
* Profile Attributes - variables
*/

// Service declaration
static CONST gattAttrType_t myAccDecl = { ATT_UUID_SIZE, myAccUUID };

// Characteristic "MyAccX" Properties (for declaration)
static uint8_t myAcc_MyAccXProps = GATT_PROP_READ | GATT_PROP_NOTIFY;

// Characteristic "MyAccX" Value variable
static uint8_t myAcc_MyAccXVal[MYACC_MYACCX_LEN] = {0};

// Characteristic "MyAccX" CCCD
static gattCharCfg_t *myAcc_MyAccXConfig;
// Characteristic "MyAccY" Properties (for declaration)
static uint8_t myAcc_MyAccYProps = GATT_PROP_READ | GATT_PROP_NOTIFY;

// Characteristic "MyAccY" Value variable
static uint8_t myAcc_MyAccYVal[MYACC_MYACCY_LEN] = {0};
static gattCharCfg_t *myAcc_MyAccYConfig;

// Characteristic "MyAccZ" Properties (for declaration)
static uint8_t myAcc_MyAccZProps = GATT_PROP_READ | GATT_PROP_NOTIFY;
static gattCharCfg_t *myAcc_MyAccZConfig;

// Characteristic "MyAccZ" Value variable
static uint8_t myAcc_MyAccZVal[MYACC_MYACCZ_LEN] = {0};

/*********************************************************************
* Profile Attributes - Table
*/

static gattAttribute_t myAccAttrTbl[] =
{
  // myAcc Service Declaration
  {
    { ATT_BT_UUID_SIZE, primaryServiceUUID },
    GATT_PERMIT_READ,
    0,
    (uint8_t *)&myAccDecl
  },
    // MyAccX Characteristic Declaration
    {
      { ATT_BT_UUID_SIZE, characterUUID },
      GATT_PERMIT_READ,
      0,
      &myAcc_MyAccXProps
    },
      // MyAccX Characteristic Value
      {
        { ATT_UUID_SIZE, myAcc_MyAccXUUID },
        GATT_PERMIT_READ,
        0,
        myAcc_MyAccXVal
      },
      // MyAccX CCCD
      {
        { ATT_BT_UUID_SIZE, clientCharCfgUUID },
        GATT_PERMIT_READ | GATT_PERMIT_WRITE,
        0,
        (uint8 *)&myAcc_MyAccXConfig
      },
    // MyAccY Characteristic Declaration
    {
      { ATT_BT_UUID_SIZE, characterUUID },
      GATT_PERMIT_READ,
      0,
      &myAcc_MyAccYProps
    },
      // MyAccY Characteristic Value
      {
        { ATT_UUID_SIZE, myAcc_MyAccYUUID },
        GATT_PERMIT_READ,
        0,
        myAcc_MyAccYVal
      },
      // MyAccY CCCD
      {
        { ATT_BT_UUID_SIZE, clientCharCfgUUID },
        GATT_PERMIT_READ | GATT_PERMIT_WRITE,
        0,
        (uint8_t *)&myAcc_MyAccYConfig
      },
    // MyAccZ Characteristic Declaration
    {
      { ATT_BT_UUID_SIZE, characterUUID },
      GATT_PERMIT_READ,
      0,
      &myAcc_MyAccZProps
    },
      // MyAccZ Characteristic Value
      {
        { ATT_UUID_SIZE, myAcc_MyAccZUUID },
        GATT_PERMIT_READ,
        0,
        myAcc_MyAccZVal
      },
      // MyAccY CCCD
      {
        { ATT_BT_UUID_SIZE, clientCharCfgUUID },
        GATT_PERMIT_READ | GATT_PERMIT_WRITE,
        0,
        (uint8_t *)&myAcc_MyAccZConfig
      },
};

/*********************************************************************
 * LOCAL FUNCTIONS
 */
static bStatus_t myAcc_ReadAttrCB( uint16_t connHandle, gattAttribute_t *pAttr,
                                           uint8_t *pValue, uint16_t *pLen, uint16_t offset,
                                           uint16_t maxLen, uint8_t method );
static bStatus_t myAcc_WriteAttrCB( uint16_t connHandle, gattAttribute_t *pAttr,
                                            uint8_t *pValue, uint16_t len, uint16_t offset,
                                            uint8_t method );

/*********************************************************************
 * PROFILE CALLBACKS
 */
// Simple Profile Service Callbacks
CONST gattServiceCBs_t myAccCBs =
{
  myAcc_ReadAttrCB,  // Read callback function pointer
  myAcc_WriteAttrCB, // Write callback function pointer
  NULL                       // Authorization callback function pointer
};

/*********************************************************************
* PUBLIC FUNCTIONS
*/

/*
 * MyAcc_AddService- Initializes the MyAcc service by registering
 *          GATT attributes with the GATT server.
 *
 */
extern bStatus_t MyAcc_AddService( uint8_t rspTaskId )
{
  uint8_t status;

  // Allocate Client Characteristic Configuration table
  myAcc_MyAccXConfig = (gattCharCfg_t *)ICall_malloc( sizeof(gattCharCfg_t) * linkDBNumConns );
  if ( myAcc_MyAccXConfig == NULL )
  {
    return ( bleMemAllocError );
  }

  // Initialize Client Characteristic Configuration attributes
  GATTServApp_InitCharCfg( LINKDB_CONNHANDLE_INVALID, myAcc_MyAccXConfig );

  myAcc_MyAccYConfig = (gattCharCfg_t *)ICall_malloc( sizeof(gattCharCfg_t) * linkDBNumConns );
  if ( myAcc_MyAccYConfig == NULL )
  {
    return ( bleMemAllocError );
  }

  // Initialize Client Characteristic Configuration attributes
  GATTServApp_InitCharCfg( INVALID_CONNHANDLE, myAcc_MyAccYConfig );

  myAcc_MyAccZConfig = (gattCharCfg_t *)ICall_malloc( sizeof(gattCharCfg_t) * linkDBNumConns );
  if ( myAcc_MyAccZConfig == NULL )
  {
    return ( bleMemAllocError );
  }

  // Initialize Client Characteristic Configuration attributes
  GATTServApp_InitCharCfg( INVALID_CONNHANDLE, myAcc_MyAccZConfig );

  // Register GATT attribute list and CBs with GATT Server App
  status = GATTServApp_RegisterService( myAccAttrTbl,
                                        GATT_NUM_ATTRS( myAccAttrTbl ),
                                        GATT_MAX_ENCRYPT_KEY_SIZE,
                                        &myAccCBs );

  return ( status );
}

/*
 * MyAcc_RegisterAppCBs - Registers the application callback function.
 *                    Only call this function once.
 *
 *    appCallbacks - pointer to application callbacks.
 */
bStatus_t MyAcc_RegisterAppCBs( myAccCBs_t *appCallbacks )
{
  if ( appCallbacks )
  {
    pAppCBs = appCallbacks;

    return ( SUCCESS );
  }
  else
  {
    return ( bleAlreadyInRequestedMode );
  }
}

/*
 * MyAcc_SetParameter - Set a MyAcc parameter.
 *
 *    param - Profile parameter ID
 *    len - length of data to right
 *    value - pointer to data to write.  This is dependent on
 *          the parameter ID and WILL be cast to the appropriate
 *          data type (example: data type of uint16 will be cast to
 *          uint16 pointer).
 */
bStatus_t MyAcc_SetParameter( uint8_t param, uint16_t len, void *value )
{
  bStatus_t ret = SUCCESS;
  Log_info0("Inside MyAcc_SetParameter");
  switch ( param )
  {
    case MYACC_MYACCX_ID:
      if ( len == MYACC_MYACCX_LEN )
      {
        memcpy(myAcc_MyAccXVal, value, len);

        // Try to send notification.
        GATTServApp_ProcessCharCfg( myAcc_MyAccXConfig, (uint8_t *)&myAcc_MyAccXVal, FALSE,
                                    myAccAttrTbl, GATT_NUM_ATTRS( myAccAttrTbl ),
                                    INVALID_TASK_ID,  myAcc_ReadAttrCB);
      }
      else
      {
        ret = bleInvalidRange;
      }
      break;
    case MYACC_MYACCY_ID:
      if ( len == MYACC_MYACCY_LEN )
      {
        memcpy(myAcc_MyAccYVal, value, len);

        // Try to send notification.
        GATTServApp_ProcessCharCfg( myAcc_MyAccYConfig, (uint8_t *)&myAcc_MyAccYVal, FALSE,
                                    myAccAttrTbl, GATT_NUM_ATTRS( myAccAttrTbl ),
                                    INVALID_TASK_ID,  myAcc_ReadAttrCB);
      }
      else
      {
        ret = bleInvalidRange;
      }
      break;
    case MYACC_MYACCZ_ID:
      if ( len == MYACC_MYACCZ_LEN )
      {
        memcpy(myAcc_MyAccZVal, value, len);

        // Try to send notification.
        GATTServApp_ProcessCharCfg( myAcc_MyAccZConfig, (uint8_t *)&myAcc_MyAccZVal, FALSE,
                                    myAccAttrTbl, GATT_NUM_ATTRS( myAccAttrTbl ),
                                    INVALID_TASK_ID,  myAcc_ReadAttrCB);
      }
      else
      {
        ret = bleInvalidRange;
      }
      break;
    default:
      ret = INVALIDPARAMETER;
      break;

  }
  return ret;
}


/*
 * MyAcc_GetParameter - Get a MyAcc parameter.
 *
 *    param - Profile parameter ID
 *    value - pointer to data to write.  This is dependent on
 *          the parameter ID and WILL be cast to the appropriate
 *          data type (example: data type of uint16 will be cast to
 *          uint16 pointer).
 */
bStatus_t MyAcc_GetParameter( uint8_t param, uint16_t *len, void *value )
{
  bStatus_t ret = SUCCESS;
  switch ( param )
  {
    default:
      ret = INVALIDPARAMETER;
      break;
  }
  return ret;
}


/*********************************************************************
 * @fn          myAcc_ReadAttrCB
 *
 * @brief       Read an attribute.
 *
 * @param       connHandle - connection message was received on
 * @param       pAttr - pointer to attribute
 * @param       pValue - pointer to data to be read
 * @param       pLen - length of data to be read
 * @param       offset - offset of the first octet to be read
 * @param       maxLen - maximum length of data to be read
 * @param       method - type of read message
 *
 * @return      SUCCESS, blePending or Failure
 */
static bStatus_t myAcc_ReadAttrCB( uint16_t connHandle, gattAttribute_t *pAttr,
                                       uint8_t *pValue, uint16_t *pLen, uint16_t offset,
                                       uint16_t maxLen, uint8_t method )
{
  bStatus_t status = SUCCESS;

  // See if request is regarding the MyAccX Characteristic Value
if ( ! memcmp(pAttr->type.uuid, myAcc_MyAccXUUID, pAttr->type.len) )
  {
    Log_info0("SetParameter in myaccx");
    if ( offset > MYACC_MYACCX_LEN )  // Prevent malicious ATT ReadBlob offsets.
    {
      Log_info0("SetParameterx in offset is bigger");
      status = ATT_ERR_INVALID_OFFSET;
    }
    else
    {
      Log_info0("SetParameterx in offset is lower");
      *pLen = MIN(maxLen, MYACC_MYACCX_LEN - offset);  // Transmit as much as possible
      memcpy(pValue, pAttr->pValue + offset, *pLen);
    }
  }
else if ( ! memcmp(pAttr->type.uuid, myAcc_MyAccYUUID, pAttr->type.len) )
  {
    Log_info0("SetParameter in myaccy");
    if ( offset > MYACC_MYACCY_LEN )  // Prevent malicious ATT ReadBlob offsets.
    {
        Log_info0("SetParametery in offset is bigger");
      status = ATT_ERR_INVALID_OFFSET;
    }
    else
    {
        Log_info0("SetParametery in offset is lower");
      *pLen = MIN(maxLen, MYACC_MYACCY_LEN - offset);  // Transmit as much as possible
      memcpy(pValue, pAttr->pValue + offset, *pLen);
    }
}
else if ( ! memcmp(pAttr->type.uuid, myAcc_MyAccZUUID, pAttr->type.len) )
  {
    Log_info0("SetParameter in myaccz");
    if ( offset > MYACC_MYACCZ_LEN )  // Prevent malicious ATT ReadBlob offsets.
    {
        Log_info0("SetParameterz in offset is lower");
      status = ATT_ERR_INVALID_OFFSET;
    }
    else
    {
        Log_info0("SetParameterz in offset is lower");
      *pLen = MIN(maxLen, MYACC_MYACCZ_LEN - offset);  // Transmit as much as possible
      memcpy(pValue, pAttr->pValue + offset, *pLen);
    }

  } else
  {
    // If we get here, that means you've forgotten to add an if clause for a
    // characteristic value attribute in the attribute table that has READ permissions.
    *pLen = 0;
    status = ATT_ERR_ATTR_NOT_FOUND;
  }

  return status;
}


/*********************************************************************
 * @fn      myAcc_WriteAttrCB
 *
 * @brief   Validate attribute data prior to a write operation
 *
 * @param   connHandle - connection message was received on
 * @param   pAttr - pointer to attribute
 * @param   pValue - pointer to data to be written
 * @param   len - length of data
 * @param   offset - offset of the first octet to be written
 * @param   method - type of write message
 *
 * @return  SUCCESS, blePending or Failure
 */
static bStatus_t myAcc_WriteAttrCB( uint16_t connHandle, gattAttribute_t *pAttr,
                                        uint8_t *pValue, uint16_t len, uint16_t offset,
                                        uint8_t method )
{
  bStatus_t status  = SUCCESS;
  uint8_t   paramID = 0xFF;

  // See if request is regarding a Client Characterisic Configuration
  if ( ! memcmp(pAttr->type.uuid, clientCharCfgUUID, pAttr->type.len) )
  {
    // Allow only notifications.
    status = GATTServApp_ProcessCCCWriteReq( connHandle, pAttr, pValue, len,
                                             offset, GATT_CLIENT_CFG_NOTIFY);
  }
  else
  {
    // If we get here, that means you've forgotten to add an if clause for a
    // characteristic value attribute in the attribute table that has WRITE permissions.
    status = ATT_ERR_ATTR_NOT_FOUND;
  }

  // Let the application know something changed (if it did) by using the
  // callback it registered earlier (if it did).
  if (paramID != 0xFF)
    if ( pAppCBs && pAppCBs->pfnChangeCb )
    {
      uint16_t svcUuid = MYACC_SERV_UUID;
      pAppCBs->pfnChangeCb(connHandle, svcUuid, paramID, len, pValue); // Call app function from stack task context.
    }
  return status;
}
