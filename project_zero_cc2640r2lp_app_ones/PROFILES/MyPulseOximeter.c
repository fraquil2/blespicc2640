/**********************************************************************************************
 * Filename:       MyPulseOximeter.c
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
#include <uartlog/UartLog.h>

#include "MyPulseOximeter.h"

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

// MyPulseOximeter Service UUID
CONST uint8_t MyPulseOximeterUUID[ATT_BT_UUID_SIZE] =
{
  LO_UINT16(MYPULSEOXIMETER_SERV_UUID), HI_UINT16(MYPULSEOXIMETER_SERV_UUID)
};

// myValue UUID
CONST uint8_t MyPulseOximeter_MyValueUUID[ATT_UUID_SIZE] =
{
  TI_BASE_UUID_128(MYPULSEOXIMETER_MYVALUE_UUID)
};

/*********************************************************************
 * LOCAL VARIABLES
 */

static MyPulseOximeterCBs_t *pAppCBs = NULL;

/*********************************************************************
* Profile Attributes - variables
*/

// Service declaration
static CONST gattAttrType_t MyPulseOximeterDecl = { ATT_BT_UUID_SIZE, MyPulseOximeterUUID };

// Characteristic "MyValue" Properties (for declaration)
static uint8_t MyPulseOximeter_MyValueProps = GATT_PROP_READ | GATT_PROP_NOTIFY;

// Characteristic "MyValue" Value variable
static uint8_t MyPulseOximeter_MyValueVal[MYPULSEOXIMETER_MYVALUE_LEN] = {0};

// Characteristic "MyValue" CCCD
static gattCharCfg_t *MyPulseOximeter_MyValueConfig;

/*********************************************************************
* Profile Attributes - Table
*/

static gattAttribute_t MyPulseOximeterAttrTbl[] =
{
  // MyPulseOximeter Service Declaration
  {
    { ATT_BT_UUID_SIZE, primaryServiceUUID },
    GATT_PERMIT_READ,
    0,
    (uint8_t *)&MyPulseOximeterDecl
  },
    // MyValue Characteristic Declaration
    {
      { ATT_BT_UUID_SIZE, characterUUID },
      GATT_PERMIT_READ,
      0,
      &MyPulseOximeter_MyValueProps
    },
      // MyValue Characteristic Value
      {
        { ATT_UUID_SIZE, MyPulseOximeter_MyValueUUID },
        GATT_PERMIT_READ,
        0,
        MyPulseOximeter_MyValueVal
      },
      // MyValue CCCD
      {
        { ATT_BT_UUID_SIZE, clientCharCfgUUID },
        GATT_PERMIT_READ | GATT_PERMIT_WRITE,
        0,
        (uint8 *)&MyPulseOximeter_MyValueConfig
      },
};

/*********************************************************************
 * LOCAL FUNCTIONS
 */
static bStatus_t MyPulseOximeter_ReadAttrCB( uint16_t connHandle, gattAttribute_t *pAttr,
                                           uint8_t *pValue, uint16_t *pLen, uint16_t offset,
                                           uint16_t maxLen, uint8_t method );
static bStatus_t MyPulseOximeter_WriteAttrCB( uint16_t connHandle, gattAttribute_t *pAttr,
                                            uint8_t *pValue, uint16_t len, uint16_t offset,
                                            uint8_t method );

/*********************************************************************
 * PROFILE CALLBACKS
 */
// Simple Profile Service Callbacks
CONST gattServiceCBs_t MyPulseOximeterCBs =
{
  MyPulseOximeter_ReadAttrCB,  // Read callback function pointer
  MyPulseOximeter_WriteAttrCB, // Write callback function pointer
  NULL                       // Authorization callback function pointer
};

/*********************************************************************
* PUBLIC FUNCTIONS
*/

/*
 * MyPulseOximeter_AddService- Initializes the MyPulseOximeter service by registering
 *          GATT attributes with the GATT server.
 *
 */
extern bStatus_t MyPulseOximeter_AddService( uint8_t rspTaskId )
{
  uint8_t status;

  // Allocate Client Characteristic Configuration table
  MyPulseOximeter_MyValueConfig = (gattCharCfg_t *)ICall_malloc( sizeof(gattCharCfg_t) * linkDBNumConns );
  if ( MyPulseOximeter_MyValueConfig == NULL )
  {
    return ( bleMemAllocError );
  }

  // Initialize Client Characteristic Configuration attributes
  GATTServApp_InitCharCfg( LINKDB_CONNHANDLE_INVALID, MyPulseOximeter_MyValueConfig );
  // Register GATT attribute list and CBs with GATT Server App
  status = GATTServApp_RegisterService( MyPulseOximeterAttrTbl,
                                        GATT_NUM_ATTRS( MyPulseOximeterAttrTbl ),
                                        GATT_MAX_ENCRYPT_KEY_SIZE,
                                        &MyPulseOximeterCBs );

  return ( status );
}

/*
 * MyPulseOximeter_RegisterAppCBs - Registers the application callback function.
 *                    Only call this function once.
 *
 *    appCallbacks - pointer to application callbacks.
 */
bStatus_t MyPulseOximeter_RegisterAppCBs( MyPulseOximeterCBs_t *appCallbacks )
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
 * MyPulseOximeter_SetParameter - Set a MyPulseOximeter parameter.
 *
 *    param - Profile parameter ID
 *    len - length of data to right
 *    value - pointer to data to write.  This is dependent on
 *          the parameter ID and WILL be cast to the appropriate
 *          data type (example: data type of uint16 will be cast to
 *          uint16 pointer).
 */
bStatus_t MyPulseOximeter_SetParameter( uint8_t param, uint16_t len, void *value )
{
  bStatus_t ret = SUCCESS;
  switch ( param )
  {
    case MYPULSEOXIMETER_MYVALUE_ID:
      if ( len == MYPULSEOXIMETER_MYVALUE_LEN )
      {
        memcpy(MyPulseOximeter_MyValueVal, value, len);
        Log_info2("SetParameter : %s value: %d", (uintptr_t)"PULSEOXIMETER", (uintptr_t)len);
        // Try to send notification.
        GATTServApp_ProcessCharCfg( MyPulseOximeter_MyValueConfig, (uint8_t *)&MyPulseOximeter_MyValueVal, FALSE,
                                    MyPulseOximeterAttrTbl, GATT_NUM_ATTRS( MyPulseOximeterAttrTbl ),
                                    INVALID_TASK_ID,  MyPulseOximeter_ReadAttrCB);
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
 * MyPulseOximeter_GetParameter - Get a MyPulseOximeter parameter.
 *
 *    param - Profile parameter ID
 *    value - pointer to data to write.  This is dependent on
 *          the parameter ID and WILL be cast to the appropriate
 *          data type (example: data type of uint16 will be cast to
 *          uint16 pointer).
 */
bStatus_t MyPulseOximeter_GetParameter( uint8_t param, uint16_t *len, void *value )
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
 * @fn          MyPulseOximeter_ReadAttrCB
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
static bStatus_t MyPulseOximeter_ReadAttrCB( uint16_t connHandle, gattAttribute_t *pAttr,
                                       uint8_t *pValue, uint16_t *pLen, uint16_t offset,
                                       uint16_t maxLen, uint8_t method )
{
  bStatus_t status = SUCCESS;

  // See if request is regarding the MyValue Characteristic Value
if ( ! memcmp(pAttr->type.uuid, MyPulseOximeter_MyValueUUID, pAttr->type.len) )
  {
    if ( offset > MYPULSEOXIMETER_MYVALUE_LEN )  // Prevent malicious ATT ReadBlob offsets.
    {
      status = ATT_ERR_INVALID_OFFSET;
    }
    else
    {
      *pLen = MIN(maxLen, MYPULSEOXIMETER_MYVALUE_LEN - offset);  // Transmit as much as possible
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
 * @fn      MyPulseOximeter_WriteAttrCB
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
static bStatus_t MyPulseOximeter_WriteAttrCB( uint16_t connHandle, gattAttribute_t *pAttr,
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
      uint16_t svcUuid = MYPULSEOXIMETER_SERV_UUID;
      pAppCBs->pfnChangeCb(connHandle, svcUuid, paramID, len, pValue); // Call app function from stack task context.
    }
  return status;
}
