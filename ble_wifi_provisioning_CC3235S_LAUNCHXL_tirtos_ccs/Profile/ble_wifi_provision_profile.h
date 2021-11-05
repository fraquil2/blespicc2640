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
#ifndef BLEWIFIPROVISIONPROFILE_H
#define BLEWIFIPROVISIONPROFILE_H

#ifdef __cplusplus
extern "C"
{
#endif
/*******************************************************************************
 *                                CONSTANTS
 ********************** ********************************************************/
#include "profile_util.h"

/*******************************************************************************
 *                                CONSTANTS
 ******************************************************************************/
/* Characteristic Types - These must be listed in order that they appear
   in service */
#define BLE_WIFI_PROV_STATUS_CHAR    0x00
#define BLE_WIFI_PROV_NOTIFY_CHAR    0x01
#define BLE_WIFI_PROV_SSID_CHAR      0x02
#define BLE_WIFI_PROV_PASS_CHAR      0x03
#define BLE_WIFI_PROV_DEVNAME_CHAR   0x04
#define BLE_WIFI_PROV_START_CHAR     0x05

#define BLE_WIFI_PROV_STATUS_ID PROFILE_ID_CREATE(BLE_WIFI_PROV_STATUS_CHAR, PROFILE_VALUE)
#define BLE_WIFI_PROV_NOTIFY_ID PROFILE_ID_CREATE(BLE_WIFI_PROV_NOTIFY_CHAR,PROFILE_VALUE)
#define BLE_WIFI_PROV_SSID_ID PROFILE_ID_CREATE(BLE_WIFI_PROV_SSID_CHAR,PROFILE_VALUE)
#define BLE_WIFI_PROV_PASS_ID PROFILE_ID_CREATE(BLE_WIFI_PROV_PASS_CHAR,PROFILE_VALUE)
#define BLE_WIFI_PROV_DEVNAME_ID PROFILE_ID_CREATE(BLE_WIFI_PROV_DEVNAME_CHAR,PROFILE_VALUE)
#define BLE_WIFI_PROV_START_ID PROFILE_ID_CREATE(BLE_WIFI_PROV_START_CHAR,PROFILE_VALUE)

/* BLE Wi-Fi Provisioning Service UUID */
#define BLE_WIFI_PROV_SERV_UUID             0xFFF0

/*  BLE Wi-Fi Provisioning  UUIDs */
#define BLE_WIFI_PROV_STATUS_UUID          0xFFF2
#define BLE_WIFI_PROV_NOTIFY_UUID          0xFFF3
#define BLE_WIFI_PROV_SSID_UUID            0xFFF4
#define BLE_WIFI_PROV_PASS_UUID            0xFFF5
#define BLE_WIFI_PROV_DEVNAME_UUID         0xFFF6
#define BLE_WIFI_PROV_START_UUID           0xFFF7

#define SERVAPP_NUM_ATTR_SUPPORTED 6

#define BLE_WIFI_MIN_SSID_LEN       1
#define BLE_WIFI_MAX_SSID_LEN       32
#define BLE_WIFI_MIN_PASS_LEN       1
#define BLE_WIFI_MAX_PASS_LEN       32
#define BLE_WIFI_MIN_DEVNAME_LEN    1
#define BLE_WIFI_MAX_DEVNAME_LEN    32


/*******************************************************************************
 *                                  FUNCTIONS
 ******************************************************************************/
/*
 * BLE_WiFi_Profile_AddService- Initializes the BLE Wi-Fi provisioning service
 * with the GATT server.
 *
 * @param   services - services to add. This is a bit map and can
 *                     contain more than one service.
 */
extern uint32_t BLE_WiFi_Profile_AddService(void);

/*
 * BLE_WiFi_Profile_RegisterAppCB - Registers the application callback function.
 *                    Only call this function once.
 *
 *    appCallbacks - pointer to application callbacks.
 */
extern uint32_t BLE_WiFi_Profile_RegisterAppCB(
        BLEProfileCallbacks_t *callbacks);

/*
 * BLE_WiFi_Profile_setParameter - Set a BLE WiFi Provision Parameter
 *
 *    param - Profile parameter ID
 *    len - length of data to right
 *    value - pointer to data to write.  This is dependent on
 *          the parameter ID and WILL be cast to the appropriate
 *          data type (example: data type of uint16_t will be cast to
 *          uint16_t pointer).
 */
extern uint32_t BLE_WiFi_Profile_setParameter(uint8 param, uint8 len,
                                              void *value);

/*
 * BLE_WiFi_Profile_getParameter - Get a BLE WiFi Provisioning Parameter
 *
 *    param - Profile parameter ID
 *    value - pointer to data to write.  This is dependent on
 *          the parameter ID and WILL be cast to the appropriate
 *          data type (example: data type of uint16_t will be cast to
 *          uint16_t pointer).
 */
extern uint32_t BLE_WiFi_Profile_getParameter(uint8 param, void *value);

#ifdef __cplusplus
}
#endif

#endif /* BLEWIFIPROVISIONPROFILE_H */

