/* --COPYRIGHT--,BSD
 * Copyright (c) 2017, Texas Instruments Incorporated
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
 * --/COPYRIGHT--*/
/*******************************************************************************
 *                                     INCLUDES
 ******************************************************************************/
#include <string.h>
#include <stdint.h>

#include <ti/sap/snp.h>
#include <ti/sap/snp_rpc.h>
#include <ti/sap/sap.h>

/* Local Includes */
#include "profile_util.h"
#include "ble_wifi_provision_profile.h"

/*******************************************************************************
 *                                  GLOBAL TYPEDEFS
 ******************************************************************************/
/* BLE WiFi Provisioning UUID: 0xFFF0 */
uint8_t bleWiFiProvServUUID[SNP_16BIT_UUID_SIZE] =
{
    LO_UINT16(BLE_WIFI_PROV_SERV_UUID),
    HI_UINT16(BLE_WIFI_PROV_SERV_UUID)
};

/* Provisioning Status UUID: 0xFFF2 */
uint8_t provStatusUUID[SNP_16BIT_UUID_SIZE] =
{
    LO_UINT16(BLE_WIFI_PROV_STATUS_UUID),
    HI_UINT16(BLE_WIFI_PROV_STATUS_UUID)
};

/* Provisioning Notify UUID: 0xFFF3 */
uint8_t provNotifyUUID[SNP_16BIT_UUID_SIZE] =
{
    LO_UINT16(BLE_WIFI_PROV_NOTIFY_UUID),
    HI_UINT16(BLE_WIFI_PROV_NOTIFY_UUID)
};

/* Provisioning SSID UUID: 0xFFF4 */
uint8_t provSSIDUUID[SNP_16BIT_UUID_SIZE] =
{
    LO_UINT16(BLE_WIFI_PROV_SSID_UUID),
    HI_UINT16(BLE_WIFI_PROV_SSID_UUID)
};

/* Provisioning Password UUID: 0xFFF5 */
uint8_t provPassUUID[SNP_16BIT_UUID_SIZE] =
{
    LO_UINT16(BLE_WIFI_PROV_PASS_UUID),
    HI_UINT16(BLE_WIFI_PROV_PASS_UUID)
};

/* Provisioning Device Name UUID: 0xFFF6 */
uint8_t provDevNameUUID[SNP_16BIT_UUID_SIZE] =
{
    LO_UINT16(BLE_WIFI_PROV_DEVNAME_UUID),
    HI_UINT16(BLE_WIFI_PROV_DEVNAME_UUID)
};

/* Provisioning Start UUID: 0xFFF7 */
uint8_t provStartUUID[SNP_16BIT_UUID_SIZE] =
{
    LO_UINT16(BLE_WIFI_PROV_START_UUID),
    HI_UINT16(BLE_WIFI_PROV_START_UUID)
};

/*******************************************************************************
 *                             LOCAL VARIABLES
 ******************************************************************************/
static CharacteristicChangeCB_t bleWiFiProfile_AppWriteCB = NULL;
static CCCDUpdateCB_t bleWiFiProfile_AppCccdCB = NULL;
static uint8_t cccdFlag = 0;
static uint16_t connHandle = 0;

/*******************************************************************************
 *                              Profile Attributes - TYPEDEFS
 ******************************************************************************/
SAP_Service_t bleWiFiService;
SAP_CharHandle_t bleWiFiChars[SERVAPP_NUM_ATTR_SUPPORTED];

static UUIDType_t bleWiFiUUID =
{SNP_16BIT_UUID_SIZE, bleWiFiProvServUUID};

/* Characteristic 1 Value */
static uint8_t provStatusValue = 0;

/* Provisioning Status User Description */
static uint8_t bleWiFiProfStatusUserDesp[] = "Provisioning Status";

/* Provisioning Notify Value */
static uint8_t provNotifyValue = 0;

/* Provisioning Notify User Description */
static uint8_t bleWiFiProfNotifyUserDesp[] = "Provisioning Notify";

/* Provisioning SSID Value */
uint8_t provSSIDValue[BLE_WIFI_MAX_SSID_LEN];

/* Provisioning SSID Current Length */
static uint8_t provCurSSIDLen = 0;

/* Provisioning SSID User Description */
static uint8_t bleWiFiProfSSIDUserDesp[] = "SSID";

/* Provisioning Password Value */
uint8_t provPassValue[BLE_WIFI_MAX_PASS_LEN];

/* Provisioning Password Current Length */
static uint8_t provCurPassLen = 0;

/* Provisioning Password User Description */
static uint8_t bleWiFiProfKeyUserDesp[] = "Security Key";

/* Provisioning Device Name Value */
uint8_t provDevNameValue[BLE_WIFI_MAX_DEVNAME_LEN];

/* Provisioning Password User Description */
static uint8_t provDevNameUserDesp[] = "Device Name";

/* Provisioning Device Name Current Length */
static uint8_t provCurDevNameLen = 0;

/* Provisioning Start Value */
static uint8_t provStartValue;

/* Provisioning Password User Description */
static uint8_t provStartUserDesp[] = "Provisioning Start";



/*******************************************************************************
 *                              Profile Attributes - TABLE
 ******************************************************************************/
SAP_UserDescAttr_t provStatusUserDesc =
{
    SNP_GATT_PERMIT_READ,
    sizeof(bleWiFiProfStatusUserDesp),
    sizeof(bleWiFiProfStatusUserDesp),
    bleWiFiProfStatusUserDesp
};

SAP_UserDescAttr_t provNotifyUserDesc =
{
    SNP_GATT_PERMIT_READ,
    sizeof(bleWiFiProfNotifyUserDesp),
    sizeof(bleWiFiProfNotifyUserDesp),
    bleWiFiProfNotifyUserDesp
};

SAP_UserCCCDAttr_t provNotifyCCCD =
{
    SNP_GATT_PERMIT_READ | SNP_GATT_PERMIT_WRITE
};

SAP_UserDescAttr_t provSSIDUserDesc =
{
    SNP_GATT_PERMIT_READ,
    sizeof(bleWiFiProfSSIDUserDesp),
    sizeof(bleWiFiProfSSIDUserDesp),
    bleWiFiProfSSIDUserDesp
};

SAP_UserDescAttr_t provPassUserDesc =
{
    SNP_GATT_PERMIT_READ,
    sizeof(bleWiFiProfKeyUserDesp),
    sizeof(bleWiFiProfKeyUserDesp),
    bleWiFiProfKeyUserDesp
};

SAP_UserDescAttr_t provDevNameUserDesc =
{
    SNP_GATT_PERMIT_READ,
    sizeof(provDevNameUserDesp),
    sizeof(provDevNameUserDesp),
    provDevNameUserDesp
};

SAP_UserDescAttr_t provStartUserDesc =
{
    SNP_GATT_PERMIT_READ,
    sizeof(provStartUserDesp),
    sizeof(provStartUserDesp),
    provStartUserDesp
};


static SAP_Char_t bleWiFiProfileAttrTbl[SERVAPP_NUM_ATTR_SUPPORTED] =
{
    /* Provisioning Status Value Declaration */
    {
        {SNP_16BIT_UUID_SIZE, provStatusUUID},          /* UUID */
        SNP_GATT_PROP_READ | SNP_GATT_PROP_WRITE,       /* Properties */
        SNP_GATT_PERMIT_READ | SNP_GATT_PERMIT_WRITE,   /* Permissions */
        &provStatusUserDesc                             /* User Description */
    },

    /* Provisioning Notify Value Declaration */
    {
        {SNP_16BIT_UUID_SIZE, provNotifyUUID},          /* UUID */
        SNP_GATT_PROP_NOTIFICATION,                     /* Properties */
        0,                                              /* Permissions */
        &provNotifyUserDesc,                            /* User Description */
        &provNotifyCCCD                                 /* CCCD */
    },

    /* Provisioning SSID Value Declaration */
    {
        {SNP_16BIT_UUID_SIZE, provSSIDUUID},            /* UUID */
        SNP_GATT_PROP_READ | SNP_GATT_PROP_WRITE,       /* Properties */
        SNP_GATT_PERMIT_ENCRYPT_READ | SNP_GATT_PERMIT_ENCRYPT_WRITE,   /* Permissions */
        &provSSIDUserDesc                               /* User Description */
    },

    /* Password Value Declaration */
    {
        {SNP_16BIT_UUID_SIZE, provPassUUID},            /* UUID */
        SNP_GATT_PROP_READ | SNP_GATT_PROP_WRITE,       /* Properties */
        SNP_GATT_PERMIT_ENCRYPT_READ | SNP_GATT_PERMIT_ENCRYPT_WRITE,   /* Permissions */
        &provPassUserDesc,                              /* User Description */
    },

    /* Device Name Value Declaration */
    {
        {SNP_16BIT_UUID_SIZE, provDevNameUUID},         /* UUID */
        SNP_GATT_PROP_READ | SNP_GATT_PROP_WRITE,       /* Properties */
        SNP_GATT_PERMIT_READ | SNP_GATT_PERMIT_WRITE,   /* Permissions */
        &provDevNameUserDesc,                           /* User Description */
    },

    /* Provisioning Start Value Declaration */
    {
        {SNP_16BIT_UUID_SIZE, provStartUUID},           /* UUID */
        SNP_GATT_PROP_READ | SNP_GATT_PROP_WRITE,       /* Properties */
        SNP_GATT_PERMIT_READ | SNP_GATT_PERMIT_WRITE,   /* Permissions */
        &provStartUserDesc,                             /* User Description */
    },



};

/*******************************************************************************
 *                                  LOCAL FUNCTIONS
 ******************************************************************************/
uint8_t BLE_WiFi_ReadAttrCB(void *context,
        uint16_t connectionHandle, uint16_t charHdl, uint16_t offset,
        uint16_t size, uint16_t *len, uint8_t *pData);

uint8_t BLE_WiFi_WriteAttrCB(void *context, uint16_t connectionHandle,
        uint16_t charHdl, uint16_t len, uint8_t *pData);

uint8_t BLE_WiFi_CCCDIndCB(void *context, uint16_t connectionHandle,
        uint16_t cccdHdl, uint8_t type, uint16_t value);

uint32_t BLE_WiFi_Profile_AddService(void)
{
    /* Reads through table, adding attributes to the NP. */
    bleWiFiService.serviceUUID = bleWiFiUUID;
    bleWiFiService.serviceType = SNP_PRIMARY_SERVICE;
    bleWiFiService.charTableLen = SERVAPP_NUM_ATTR_SUPPORTED;
    bleWiFiService.charTable = bleWiFiProfileAttrTbl;
    bleWiFiService.context = NULL;
    bleWiFiService.charReadCallback = BLE_WiFi_ReadAttrCB;
    bleWiFiService.charWriteCallback = BLE_WiFi_WriteAttrCB;
    bleWiFiService.cccdIndCallback = BLE_WiFi_CCCDIndCB;
    bleWiFiService.charAttrHandles = bleWiFiChars;

    /* Service is setup, register with GATT server on the SNP. */
    SAP_registerService(&bleWiFiService);

    return BLE_PROFILE_SUCCESS;
}

uint32_t BLE_WiFi_Profile_RegisterAppCB(BLEProfileCallbacks_t *callbacks)
{
    bleWiFiProfile_AppWriteCB = callbacks->charChangeCB;
    bleWiFiProfile_AppCccdCB = callbacks->cccdUpdateCB;

    return BLE_PROFILE_SUCCESS;
}

uint32_t BLE_WiFi_Profile_setParameter(uint8_t param, uint8_t len, void *value)
{
    uint_fast8_t ret = BLE_PROFILE_SUCCESS;
    snpNotifIndReq_t localReq;

    switch (PROFILE_ID_CHAR(param))
    {
    case BLE_WIFI_PROV_STATUS_CHAR:
        if (len == sizeof(uint8_t))
        {
            provStatusValue = *((uint8*) value);
        }
        else
        {
            ret = BLE_PROFILE_INVALID_RANGE;
        }
        break;
    case BLE_WIFI_PROV_SSID_CHAR:
        if ((len <= BLE_WIFI_MAX_SSID_LEN) && (len >= BLE_WIFI_MIN_SSID_LEN))
        {
            memcpy(provSSIDValue, value, len);
            provCurSSIDLen = len;
        }
        else
        {
            ret = BLE_PROFILE_INVALID_RANGE;
        }
        break;
    case BLE_WIFI_PROV_PASS_CHAR:
        if ((len <= BLE_WIFI_MAX_PASS_LEN) && (len >= BLE_WIFI_MIN_PASS_LEN))
        {
            memcpy(provPassValue, value, len);
            provCurPassLen = len;
        }
        else
        {
            ret = BLE_PROFILE_INVALID_RANGE;
        }
        break;

    case BLE_WIFI_PROV_DEVNAME_CHAR:
        if ((len <= BLE_WIFI_MAX_DEVNAME_LEN)
                && (len >= BLE_WIFI_MIN_DEVNAME_LEN))
        {
            memcpy(provDevNameValue, value, len);
            provCurDevNameLen = len;
        }
        else
        {
            ret = BLE_PROFILE_INVALID_RANGE;
        }
        break;

    case BLE_WIFI_PROV_START_CHAR:
        if (len == sizeof(uint8_t))
        {
            provStartValue = *((uint8*) value);
        }
        else
        {
            ret = BLE_PROFILE_INVALID_RANGE;
        }
        break;

    case BLE_WIFI_PROV_NOTIFY_CHAR:
        if (len == sizeof(uint8_t))
        {
            provNotifyValue = *((uint8*) value);

            /* Initialize Request */
            localReq.connHandle = connHandle;
            localReq.attrHandle = ProfileUtil_getHdlFromCharID(
                    PROFILE_ID_CREATE(BLE_WIFI_PROV_NOTIFY_CHAR, PROFILE_VALUE),
                    bleWiFiChars, SERVAPP_NUM_ATTR_SUPPORTED);
            localReq.pData = (uint8_t *) &provNotifyValue;
            localReq.authenticate = 0;

            /* Check for whether a notification or indication should be sent.
               Both flags should never be allowed to be set by NWP */
            if (cccdFlag & SNP_GATT_CLIENT_CFG_NOTIFY)
            {
                localReq.type = SNP_SEND_NOTIFICATION;
                SNP_RPC_sendNotifInd(&localReq, sizeof(provNotifyValue));
            }
            else if (cccdFlag & SNP_GATT_CLIENT_CFG_INDICATE)
            {
                localReq.type = SNP_SEND_INDICATION;
                SNP_RPC_sendNotifInd(&localReq, sizeof(provNotifyValue));
            }
        }
        else
        {
            ret = BLE_PROFILE_INVALID_RANGE;
        }
        break;
     default:
        ret = BLE_PROFILE_INVALIDPARAMETER;
        break;
    }

    return ret;
}

uint32_t BLE_WiFi_Profile_getParameter(uint8_t param, void *value)
{
    uint_fast8_t ret = BLE_PROFILE_SUCCESS;
    switch (PROFILE_ID_CHAR(param))
    {
    case BLE_WIFI_PROV_STATUS_CHAR:
        *((uint8*) value) = provStatusValue;
        break;

    case BLE_WIFI_PROV_NOTIFY_CHAR:
        *((uint8*) value) = provNotifyValue;
        break;

    case BLE_WIFI_PROV_SSID_CHAR:
        memcpy(value, (void*)provSSIDValue, provCurSSIDLen);
        break;

    case BLE_WIFI_PROV_PASS_CHAR:
        memcpy(value, (void*)provPassValue, provCurPassLen);
        break;

    case BLE_WIFI_PROV_DEVNAME_CHAR:
        memcpy(value, (void*)provDevNameValue, provCurDevNameLen);
        break;

    case BLE_WIFI_PROV_START_CHAR:
        *((uint8*) value) = provStartValue;
        break;

    default:
        ret = BLE_PROFILE_INVALIDPARAMETER;
        break;
    }

    return ret;
}

/*******************************************************************************
 * @fn          BLE_WiFi_ReadAttrCB
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
 * @return      BLE_PROFILE_SUCCESS, blePending or Failure
 ******************************************************************************/
uint8_t BLE_WiFi_ReadAttrCB(void *context, uint16_t connectionHandle,
                            uint16_t charHdl, uint16_t offset, uint16_t size,
                            uint16_t * pLen, uint8_t *pData)
{
    /* Get characteristic from handle */
    uint8_t charID = ProfileUtil_getCharIDFromHdl(charHdl, bleWiFiChars,
    SERVAPP_NUM_ATTR_SUPPORTED);
    uint8_t isValid = 0;

    /* Update connection handle (assumes one connection) */
    connHandle = connectionHandle;

    switch (PROFILE_ID_CHAR(charID))
    {
    case BLE_WIFI_PROV_STATUS_CHAR:
        switch (PROFILE_ID_CHARTYPE(charID))
        {
        case PROFILE_VALUE:
            *pLen = sizeof(provStatusValue);
            memcpy(pData, &provStatusValue, sizeof(provStatusValue));
            isValid = 1;
            break;

        default:
            /* Should not receive SP_USERDESC || SP_FORMAT || SP_CCCD */
            break;
        }
        break;
    case BLE_WIFI_PROV_SSID_CHAR:
        switch (PROFILE_ID_CHARTYPE(charID))
        {
        case PROFILE_VALUE:
            *pLen = provCurSSIDLen;
            memcpy(pData, &provSSIDValue, provCurSSIDLen);
            isValid = 1;
            break;

        default:
            /* Should not receive SP_USERDESC || SP_FORMAT || SP_CCCD */
            break;
        }
        break;
    case BLE_WIFI_PROV_PASS_CHAR:
        switch (PROFILE_ID_CHARTYPE(charID))
        {
        case PROFILE_VALUE:
            *pLen = provCurPassLen;
            memcpy(pData, &provPassValue, provCurPassLen);
            isValid = 1;
            break;

        default:
            /* Should not receive SP_USERDESC || SP_FORMAT || SP_CCCD */
            break;
        }
        break;

    case BLE_WIFI_PROV_DEVNAME_CHAR:
        switch (PROFILE_ID_CHARTYPE(charID))
        {
        case PROFILE_VALUE:
            *pLen = provCurDevNameLen;
            memcpy(pData, &provDevNameValue, provCurDevNameLen);
            isValid = 1;
            break;

        default:
            /* Should not receive SP_USERDESC || SP_FORMAT || SP_CCCD */
            break;
        }
        break;
    case BLE_WIFI_PROV_START_CHAR:
        switch (PROFILE_ID_CHARTYPE(charID))
        {
        case PROFILE_VALUE:
            *pLen = sizeof(provStartValue);
            memcpy(pData, &provStartValue, sizeof(provStartValue));
            isValid = 1;
            break;

        default:
            /* Should not receive SP_USERDESC || SP_FORMAT || SP_CCCD */
            break;
        }
        break;
    default:
        /* Should not receive SP_CHAR3 || SP_CHAR4 reads */
        break;
    }

    if (isValid)
    {
        return SNP_SUCCESS;
    }

    /* Unable to find handle - set len to 0 and return error code */
    *pLen = 0;
    return SNP_UNKNOWN_ATTRIBUTE;
}

/*******************************************************************************
 * @fn      BLE_WiFi_WriteAttrCB
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
 * @return  BLE_PROFILE_SUCCESS, blePending or Failure
 ******************************************************************************/
uint8_t BLE_WiFi_WriteAttrCB(void *context, uint16_t connectionHandle,
                             uint16_t charHdl, uint16_t len, uint8_t *pData)
{
    uint_fast8_t status = SNP_UNKNOWN_ATTRIBUTE;
    uint8_t notifyApp = PROFILE_UNKNOWN_CHAR;

    /* Update connection Handle (assumes one connection) */
    connHandle = connectionHandle;

    /* Get characteristic from handle */
    uint8_t charID = ProfileUtil_getCharIDFromHdl(charHdl, bleWiFiChars,
    SERVAPP_NUM_ATTR_SUPPORTED);

    switch (PROFILE_ID_CHAR(charID))
    {
    case BLE_WIFI_PROV_STATUS_CHAR:
        switch (PROFILE_ID_CHARTYPE(charID))
        {
        case PROFILE_VALUE:
            if (len == sizeof(provStatusValue))
            {
                provStatusValue = pData[0];
                status = SNP_SUCCESS;
                notifyApp = BLE_WIFI_PROV_STATUS_ID;
            }
            break;
        default:
            break;
        }
        break;
    case BLE_WIFI_PROV_SSID_CHAR:
        switch (PROFILE_ID_CHARTYPE(charID))
        {
        case PROFILE_VALUE:
            if ((len <= BLE_WIFI_MAX_SSID_LEN)
                    && (len >= BLE_WIFI_MIN_SSID_LEN))
            {
                memcpy(provSSIDValue, pData, len);
                provSSIDValue[len] = '\0';
                provCurSSIDLen = len + 1;
                status = SNP_SUCCESS;
                notifyApp = BLE_WIFI_PROV_SSID_ID;
            }
            break;
        default:
            break;
        }
        break;
    case BLE_WIFI_PROV_PASS_CHAR:
        switch (PROFILE_ID_CHARTYPE(charID))
        {
        case PROFILE_VALUE:
            if ((len <= BLE_WIFI_MAX_PASS_LEN)
                    && (len >= BLE_WIFI_MIN_PASS_LEN))
            {
                memcpy(provPassValue, pData, len);
                provPassValue[len] = '\0';
                provCurPassLen = len + 1;
                status = SNP_SUCCESS;
                notifyApp = BLE_WIFI_PROV_PASS_ID;
            }
            break;
        default:
            break;
        }
        break;
    case BLE_WIFI_PROV_DEVNAME_CHAR:
        switch (PROFILE_ID_CHARTYPE(charID))
        {
        case PROFILE_VALUE:
            if ((len <= BLE_WIFI_MAX_DEVNAME_LEN)
                    && (len >= BLE_WIFI_MIN_DEVNAME_LEN))
            {
                memcpy(provDevNameValue, pData, len);
                provDevNameValue[len] = '\0';
                provCurDevNameLen = len + 1;
                status = SNP_SUCCESS;
                notifyApp = BLE_WIFI_PROV_DEVNAME_ID;
            }
            break;
        default:
            break;
        }
        break;
    case BLE_WIFI_PROV_START_CHAR:
        switch (PROFILE_ID_CHARTYPE(charID))
        {
        case PROFILE_VALUE:
            if (len == sizeof(provStartValue))
            {
                provStartValue = pData[0];
                status = SNP_SUCCESS;
                notifyApp = BLE_WIFI_PROV_START_ID;
            }
            break;
        default:
            break;
        }
        break;
    default:
        /* Should not receive any other writes */
        break;
    }

    /* If a characteristic value changed then callback function to notify
     * application of change */
    if ((notifyApp != PROFILE_UNKNOWN_CHAR) && bleWiFiProfile_AppWriteCB)
    {
        bleWiFiProfile_AppWriteCB(notifyApp);
    }

    return status;
}

/*******************************************************************************
 * @fn      BLE_WiFi_CCCDIndCB
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
 * @return  BLE_PROFILE_SUCCESS, blePending or Failure
 ******************************************************************************/
uint8_t BLE_WiFi_CCCDIndCB(void *context, uint16_t connectionHandle,
        uint16_t cccdHdl, uint8_t type, uint16_t value)
{
    uint_fast8_t status = SNP_UNKNOWN_ATTRIBUTE;
    uint_fast8_t notifyApp = PROFILE_UNKNOWN_CHAR;

    /* Update connection handle (assumes one connection) */
    connHandle = connectionHandle;

    /* Get characteristic from handle */
    uint8_t charID = ProfileUtil_getCharIDFromHdl(cccdHdl, bleWiFiChars,
            SERVAPP_NUM_ATTR_SUPPORTED);

    switch (PROFILE_ID_CHAR(charID))
    {
    case BLE_WIFI_PROV_NOTIFY_CHAR:
        switch (PROFILE_ID_CHARTYPE(charID))
        {
        case PROFILE_CCCD:
            /* Set Global cccd Flag which will be used to to gate Indications
               or notifications when SetParameter() is called */
            cccdFlag = value;
            notifyApp = charID;
            status = SNP_SUCCESS;
            break;
        default:
            /* Should not receive SP_VALUE || SP_USERDESC || SP_FORMAT */
            break;
        }
        break;
    default:
        /* No other Characteristics have CCCB attributes */
        break;
    }

    /* If a characteristic value changed then callback function to notify
     * application of change.
     */
    if ((notifyApp != PROFILE_UNKNOWN_CHAR) && bleWiFiProfile_AppCccdCB)
    {
        bleWiFiProfile_AppCccdCB(notifyApp, value);
    }

    return (status);
}
