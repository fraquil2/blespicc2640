/**********************************************************************************************
 * Filename:       myGyro.c
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

#include "myGyro.h"

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

// myGyro Service UUID
CONST uint8_t myGyroUUID[ATT_UUID_SIZE] =
{
  TI_BASE_UUID_128(MYGYRO_SERV_UUID)
};

// myGyroX UUID
CONST uint8_t myGyro_MyGyroXUUID[ATT_UUID_SIZE] =
{
  TI_BASE_UUID_128(MYGYRO_MYGYROX_UUID)
};
// myGyroY UUID
CONST uint8_t myGyro_MyGyroYUUID[ATT_UUID_SIZE] =
{
  TI_BASE_UUID_128(MYGYRO_MYGYROY_UUID)
};
// myGyroZ UUID
CONST uint8_t myGyro_MyGyroZUUID[ATT_UUID_SIZE] =
{
  TI_BASE_UUID_128(MYGYRO_MYGYROZ_UUID)
};

/*********************************************************************
 * LOCAL VARIABLES
 */

static myGyroCBs_t *pAppCBs = NULL;

/*********************************************************************
* Profile Attributes - variables
*/

// Service declaration
static CONST gattAttrType_t myGyroDecl = { ATT_UUID_SIZE, myGyroUUID };

// Characteristic "MyGyroX" Properties (for declaration)
static uint8_t myGyro_MyGyroXProps = GATT_PROP_READ | GATT_PROP_NOTIFY;

// Characteristic "MyGyroX" Value variable
static uint8_t myGyro_MyGyroXVal[MYGYRO_MYGYROX_LEN] = {0};

// Characteristic "MyGyroX" CCCD
static gattCharCfg_t *myGyro_MyGyroXConfig;
// Characteristic "MyGyroY" Properties (for declaration)
static uint8_t myGyro_MyGyroYProps = GATT_PROP_READ | GATT_PROP_NOTIFY;

// Characteristic "MyGyroY" Value variable
static uint8_t myGyro_MyGyroYVal[MYGYRO_MYGYROY_LEN] = {0};
static gattCharCfg_t *myGyro_MyGyroYConfig;

// Characteristic "MyGyroZ" Properties (for declaration)
static uint8_t myGyro_MyGyroZProps = GATT_PROP_READ | GATT_PROP_NOTIFY;

// Characteristic "MyGyroZ" Value variable
static uint8_t myGyro_MyGyroZVal[MYGYRO_MYGYROZ_LEN] = {0};
static gattCharCfg_t *myGyro_MyGyroZConfig;

/*********************************************************************
* Profile Attributes - Table
*/

static gattAttribute_t myGyroAttrTbl[] =
{
  // myGyro Service Declaration
  {
    { ATT_BT_UUID_SIZE, primaryServiceUUID },
    GATT_PERMIT_READ,
    0,
    (uint8_t *)&myGyroDecl
  },
    // MyGyroX Characteristic Declaration
    {
      { ATT_BT_UUID_SIZE, characterUUID },
      GATT_PERMIT_READ,
      0,
      &myGyro_MyGyroXProps
    },
      // MyGyroX Characteristic Value
      {
        { ATT_UUID_SIZE, myGyro_MyGyroXUUID },
        GATT_PERMIT_READ,
        0,
        myGyro_MyGyroXVal
      },
      // MyGyroX CCCD
      {
        { ATT_BT_UUID_SIZE, clientCharCfgUUID },
        GATT_PERMIT_READ | GATT_PERMIT_WRITE,
        0,
        (uint8 *)&myGyro_MyGyroXConfig
      },
    // MyGyroY Characteristic Declaration
    {
      { ATT_BT_UUID_SIZE, characterUUID },
      GATT_PERMIT_READ,
      0,
      &myGyro_MyGyroYProps
    },
      // MyGyroY Characteristic Value
      {
        { ATT_UUID_SIZE, myGyro_MyGyroYUUID },
        GATT_PERMIT_READ,
        0,
        myGyro_MyGyroYVal
      },
      // MyGyroY CCCD
      {
        { ATT_BT_UUID_SIZE, clientCharCfgUUID },
        GATT_PERMIT_READ | GATT_PERMIT_WRITE,
        0,
        (uint8 *)&myGyro_MyGyroYConfig
      },
    // MyGyroZ Characteristic Declaration
    {
      { ATT_BT_UUID_SIZE, characterUUID },
      GATT_PERMIT_READ,
      0,
      &myGyro_MyGyroZProps
    },
      // MyGyroZ Characteristic Value
      {
        { ATT_UUID_SIZE, myGyro_MyGyroZUUID },
        GATT_PERMIT_READ,
        0,
        myGyro_MyGyroZVal
      },
      // MyGyroZ CCCD
      {
        { ATT_BT_UUID_SIZE, clientCharCfgUUID },
        GATT_PERMIT_READ | GATT_PERMIT_WRITE,
        0,
        (uint8 *)&myGyro_MyGyroZConfig
      },
};

/*********************************************************************
 * LOCAL FUNCTIONS
 */
static bStatus_t myGyro_ReadAttrCB( uint16_t connHandle, gattAttribute_t *pAttr,
                                           uint8_t *pValue, uint16_t *pLen, uint16_t offset,
                                           uint16_t maxLen, uint8_t method );
static bStatus_t myGyro_WriteAttrCB( uint16_t connHandle, gattAttribute_t *pAttr,
                                            uint8_t *pValue, uint16_t len, uint16_t offset,
                                            uint8_t method );

/*********************************************************************
 * PROFILE CALLBACKS
 */
// Simple Profile Service Callbacks
CONST gattServiceCBs_t myGyroCBs =
{
  myGyro_ReadAttrCB,  // Read callback function pointer
  myGyro_WriteAttrCB, // Write callback function pointer
  NULL                       // Authorization callback function pointer
};

/*********************************************************************
* PUBLIC FUNCTIONS
*/

/*
 * MyGyro_AddService- Initializes the MyGyro service by registering
 *          GATT attributes with the GATT server.
 *
 */
extern bStatus_t MyGyro_AddService( uint8_t rspTaskId )
{
  uint8_t status;

  // Allocate Client Characteristic Configuration table
  myGyro_MyGyroXConfig = (gattCharCfg_t *)ICall_malloc( sizeof(gattCharCfg_t) * linkDBNumConns );
  if ( myGyro_MyGyroXConfig == NULL )
  {
    return ( bleMemAllocError );
  }

  // Initialize Client Characteristic Configuration attributes
  GATTServApp_InitCharCfg( LINKDB_CONNHANDLE_INVALID, myGyro_MyGyroXConfig );

  myGyro_MyGyroYConfig = (gattCharCfg_t *)ICall_malloc( sizeof(gattCharCfg_t) * linkDBNumConns );
  if ( myGyro_MyGyroYConfig == NULL )
  {
    return ( bleMemAllocError );
  }

  // Initialize Client Characteristic Configuration attributes
  GATTServApp_InitCharCfg( INVALID_CONNHANDLE, myGyro_MyGyroYConfig );

  myGyro_MyGyroZConfig = (gattCharCfg_t *)ICall_malloc( sizeof(gattCharCfg_t) * linkDBNumConns );
  if ( myGyro_MyGyroZConfig == NULL )
  {
    return ( bleMemAllocError );
  }

  // Initialize Client Characteristic Configuration attributes
  GATTServApp_InitCharCfg( INVALID_CONNHANDLE, myGyro_MyGyroZConfig );
  // Register GATT attribute list and CBs with GATT Server App
  status = GATTServApp_RegisterService( myGyroAttrTbl,
                                        GATT_NUM_ATTRS( myGyroAttrTbl ),
                                        GATT_MAX_ENCRYPT_KEY_SIZE,
                                        &myGyroCBs );

  return ( status );
}

/*
 * MyGyro_RegisterAppCBs - Registers the application callback function.
 *                    Only call this function once.
 *
 *    appCallbacks - pointer to application callbacks.
 */
bStatus_t MyGyro_RegisterAppCBs( myGyroCBs_t *appCallbacks )
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
 * MyGyro_SetParameter - Set a MyGyro parameter.
 *
 *    param - Profile parameter ID
 *    len - length of data to right
 *    value - pointer to data to write.  This is dependent on
 *          the parameter ID and WILL be cast to the appropriate
 *          data type (example: data type of uint16 will be cast to
 *          uint16 pointer).
 */
bStatus_t MyGyro_SetParameter( uint8_t param, uint16_t len, void *value )
{
  bStatus_t ret = SUCCESS;
  switch ( param )
  {
    case MYGYRO_MYGYROX_ID:
      if ( len == MYGYRO_MYGYROX_LEN )
      {
        memcpy(myGyro_MyGyroXVal, value, len);

        // Try to send notification.
        GATTServApp_ProcessCharCfg( myGyro_MyGyroXConfig, (uint8_t *)&myGyro_MyGyroXVal, FALSE,
                                    myGyroAttrTbl, GATT_NUM_ATTRS( myGyroAttrTbl ),
                                    INVALID_TASK_ID,  myGyro_ReadAttrCB);
      }
      else
      {
        ret = bleInvalidRange;
      }
      break;
    case MYGYRO_MYGYROY_ID:
      if ( len == MYGYRO_MYGYROY_LEN )
      {
        memcpy(myGyro_MyGyroYVal, value, len);

        // Try to send notification.
        GATTServApp_ProcessCharCfg( myGyro_MyGyroYConfig, (uint8_t *)&myGyro_MyGyroYVal, FALSE,
                                    myGyroAttrTbl, GATT_NUM_ATTRS( myGyroAttrTbl ),
                                    INVALID_TASK_ID,  myGyro_ReadAttrCB);
      }
      else
      {
        ret = bleInvalidRange;
      }
      break;
    case MYGYRO_MYGYROZ_ID:
      if ( len == MYGYRO_MYGYROZ_LEN )
      {
        memcpy(myGyro_MyGyroZVal, value, len);

        // Try to send notification.
        GATTServApp_ProcessCharCfg( myGyro_MyGyroZConfig, (uint8_t *)&myGyro_MyGyroZVal, FALSE,
                                    myGyroAttrTbl, GATT_NUM_ATTRS( myGyroAttrTbl ),
                                    INVALID_TASK_ID,  myGyro_ReadAttrCB);
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
 * MyGyro_GetParameter - Get a MyGyro parameter.
 *
 *    param - Profile parameter ID
 *    value - pointer to data to write.  This is dependent on
 *          the parameter ID and WILL be cast to the appropriate
 *          data type (example: data type of uint16 will be cast to
 *          uint16 pointer).
 */
bStatus_t MyGyro_GetParameter( uint8_t param, uint16_t *len, void *value )
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
 * @fn          myGyro_ReadAttrCB
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
static bStatus_t myGyro_ReadAttrCB( uint16_t connHandle, gattAttribute_t *pAttr,
                                       uint8_t *pValue, uint16_t *pLen, uint16_t offset,
                                       uint16_t maxLen, uint8_t method )
{
  bStatus_t status = SUCCESS;

  // See if request is regarding the MyGyroX Characteristic Value
if ( ! memcmp(pAttr->type.uuid, myGyro_MyGyroXUUID, pAttr->type.len) )
  {
    if ( offset > MYGYRO_MYGYROX_LEN )  // Prevent malicious ATT ReadBlob offsets.
    {
      status = ATT_ERR_INVALID_OFFSET;
    }
    else
    {
      *pLen = MIN(maxLen, MYGYRO_MYGYROX_LEN - offset);  // Transmit as much as possible
      memcpy(pValue, pAttr->pValue + offset, *pLen);
    }
  }
else if ( ! memcmp(pAttr->type.uuid, myGyro_MyGyroYUUID, pAttr->type.len) )
  {
    if ( offset > MYGYRO_MYGYROY_LEN )  // Prevent malicious ATT ReadBlob offsets.
    {
      status = ATT_ERR_INVALID_OFFSET;
    }
    else
    {
      *pLen = MIN(maxLen, MYGYRO_MYGYROY_LEN - offset);  // Transmit as much as possible
      memcpy(pValue, pAttr->pValue + offset, *pLen);
    }
  }
else if ( ! memcmp(pAttr->type.uuid, myGyro_MyGyroZUUID, pAttr->type.len) )
  {
    if ( offset > MYGYRO_MYGYROZ_LEN )  // Prevent malicious ATT ReadBlob offsets.
    {
      status = ATT_ERR_INVALID_OFFSET;
    }
    else
    {
      *pLen = MIN(maxLen, MYGYRO_MYGYROZ_LEN - offset);  // Transmit as much as possible
      memcpy(pValue, pAttr->pValue + offset, *pLen);
    }
  }

  else
  {
    // If we get here, that means you've forgotten to add an if clause for a
    // characteristic value attribute in the attribute table that has READ permissions.
    *pLen = 0;
    status = ATT_ERR_ATTR_NOT_FOUND;
  }

  return status;
}


/*********************************************************************
 * @fn      myGyro_WriteAttrCB
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
static bStatus_t myGyro_WriteAttrCB( uint16_t connHandle, gattAttribute_t *pAttr,
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
      uint16_t svcUuid = MYGYRO_SERV_UUID;
      pAppCBs->pfnChangeCb(connHandle, svcUuid, paramID, len, pValue); // Call app function from stack task context.
    }
  return status;
}
