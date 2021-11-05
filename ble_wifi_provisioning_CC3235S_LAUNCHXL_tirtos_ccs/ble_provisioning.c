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

//****************************************************************************
//
//! \addtogroup
//! @{
//
//****************************************************************************
/* Standard Include */
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <mqueue.h>
#include <time.h>
#include <stdbool.h>

/* TI-DRIVERS Header files */
#include <ti/drivers/net/wifi/simplelink.h>
#include <ti/drivers/GPIO.h>
#include <ti/display/Display.h>
#include <ti/drivers/Timer.h>
#include <ti/sbl/sbl.h>
#include <ti/sbl/sbl_image.h>
#include <ti/devices/cc32xx/inc/hw_ints.h>
#include <ti/devices/cc32xx/inc/hw_memmap.h>
#include <ti/devices/cc32xx/inc/hw_types.h>
#include <ti/devices/cc32xx/driverlib/rom.h>
#include <ti/devices/cc32xx/driverlib/rom_map.h>
#include <ti/devices/cc32xx/driverlib/prcm.h>
#include <semaphore.h>

/* Local Definitions */
#include "Profile/ble_wifi_provision_profile.h"
#include "Profile/profile_util.h"
#include "sbl_wifi_snp_update.h"
#include "ap_provisioning.h"
#include "ble_provisioning.h"
#include "Board.h"

/*******************************************************************************
 Function Prototypes
 ******************************************************************************/
static void BLE_keyHandler(void);
static void BLE_SPWriteCB(uint8_t charID);
static void BLE_SPcccdCB(uint8_t charID, uint16_t value);
static void BLE_asyncCB(uint8_t cmd1, void *pParams);
static void BLE_initServices(void);
static void BLE_processSNPEventCB(uint16_t event, snpEventParam_t *param);
static void BLE_timerHandler(Timer_Handle handle);
static bool WiFi_startConnection(void);
static bool WiFi_obtainIPAddress(void);
static bool WiFi_pingGateway(uint32_t bytes);

/*******************************************************************************
 Variables
 ******************************************************************************/
/* Used for log messages */
extern Display_Handle displayOut;

/* Communication queues */
static mqd_t bleQueueRec;
static mqd_t bleQueueSend;
static mqd_t wifiQueueRec;
static mqd_t wifiQueueSend;

static BLEProfileCallbacks_t bleCallbacks = { BLE_SPWriteCB, BLE_SPcccdCB };
static char nwpstr[] = "NWP:  0xFFFFFFFFFFFF";
static char peerstr[] = "Peer: 0xFFFFFFFFFFFF";

/* Timer instance for switch three */
static Timer_Handle buttonTimer;
static Timer_Params timerParams;
static bool timerIsRunning = false;
static bool threeSecondButtonPress = false;

/* Connection Handle - Only one device currently allowed to connect to SNP */
static uint16_t connHandle = BLE_DEFAULT_CONN_HANDLE;

/* Device Name */
static uint8_t snpDeviceName[] = { 'C', 'C', '3', '2', 'X', 'X', ' ', 'W', 'i',
                                   'F', 'i' };

/* GAP - SCAN RSP data (max size = 31 bytes) */
static uint8_t scanRspData[] = {
/* Complete Name */
0x0C,/* length of this data */
SAP_GAP_ADTYPE_LOCAL_NAME_COMPLETE, 'C', 'C',
'3', '2', 'X', 'X', ' ', 'W', 'i', 'F', 'i',

/* Connection interval range */
0x05, /* length of this data */
0x12, /* GAP_ADTYPE_SLAVE_CONN_INTERVAL_RANGE, */
LO_UINT16(DEFAULT_DESIRED_MIN_CONN_INTERVAL),
HI_UINT16(DEFAULT_DESIRED_MIN_CONN_INTERVAL),
LO_UINT16(DEFAULT_DESIRED_MAX_CONN_INTERVAL),
HI_UINT16(DEFAULT_DESIRED_MAX_CONN_INTERVAL),
0x02, /* length of this data */
0x0A, /* GAP_ADTYPE_POWER_LEVEL, */
0 /* 0dBm */
};

/* GAP - Advertisement data (max size = 31 bytes, though this is
 * best kept short to conserve power while advertising) */
static uint8_t advertData[] = {
    /* Flags; this sets the device to use limited discoverable
     * mode (advertises for 30 seconds at a time) instead of general
     * discoverable mode (advertises indefinitely) */
    0x02, /* length of this data */
    SAP_GAP_ADTYPE_FLAGS, DEFAULT_DISCOVERABLE_MODE
            | SAP_GAP_ADTYPE_FLAGS_BREDR_NOT_SUPPORTED,

    /* Service UUID. This will notify the central device that we are
     * a BLE WiFi Provisioner  */
    0x03,
    SAP_GAP_ADTYPE_16BIT_MORE, LO_UINT16(BLE_WIFI_PROV_SERV_UUID),
    HI_UINT16(BLE_WIFI_PROV_SERV_UUID), };

/* Provisioning Variables */
static uint8_t gatewayIPString[20];
static uint8_t resolvedIPString[20];
static bool sblUpdateInitiated;

/* Used for switching between provisioning modes */
static bool switchProvisioningMode = false;
extern Provisioning_CB Provisioning_ControlBlock;
extern sem_t AP_connectionAsyncSem;

const char *Roles[] = { "STA", "STA", "AP", "P2P" };
const char *WlanStatus[] = { "DISCONNECTED", "SCANING", "CONNECTING",
                             "CONNECTED" };

/******************************************************************************
 SimpleLink Callback Functions
 *****************************************************************************/
//*****************************************************************************
//
//! \brief The Function Handles WLAN Events
//!
//! \param[in]  pWlanEvent - Pointer to WLAN Event Info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkWlanEventHandler(SlWlanEvent_t *pWlanEvent)
{
    switch (pWlanEvent->Id)
    {
    case SL_WLAN_EVENT_CONNECT:
    {
        SET_STATUS_BIT(Provisioning_ControlBlock.status,
                       AppStatusBits_Connection);
        CLR_STATUS_BIT(Provisioning_ControlBlock.status,
                       AppStatusBits_IpAcquired);
        CLR_STATUS_BIT(Provisioning_ControlBlock.status,
                       AppStatusBits_Ipv6lAcquired);
        CLR_STATUS_BIT(Provisioning_ControlBlock.status,
                       AppStatusBits_Ipv6gAcquired);

        /* Information about the connected AP (like name, MAC etc) will be
         * available in 'slWlanConnectAsyncResponse_t'-Applications
         * can use it if required:
         *
         * slWlanConnectAsyncResponse_t *pEventData = NULL;
         * pEventData = &pWlanEvent->EventData.STAandP2PModeWlanConnected;
         */

        /* Copy new connection SSID and BSSID to global parameters */
        memcpy(Provisioning_ControlBlock.connectionSSID,
               pWlanEvent->Data.Connect.SsidName,
               pWlanEvent->Data.Connect.SsidLen);

        Provisioning_ControlBlock.ssidLen = pWlanEvent->Data.Connect.SsidLen;

        memcpy(Provisioning_ControlBlock.connectionBSSID,
               pWlanEvent->Data.Connect.Bssid,
        SL_WLAN_BSSID_LENGTH);

        Display_printf(displayOut, 0, 0,
                       "[WLAN EVENT] STA Connected to the AP: %s ,"
                       "BSSID: %x:%x:%x:%x:%x:%x",
                       Provisioning_ControlBlock.connectionSSID,
                       Provisioning_ControlBlock.connectionBSSID[0],
                       Provisioning_ControlBlock.connectionBSSID[1],
                       Provisioning_ControlBlock.connectionBSSID[2],
                       Provisioning_ControlBlock.connectionBSSID[3],
                       Provisioning_ControlBlock.connectionBSSID[4],
                       Provisioning_ControlBlock.connectionBSSID[5]);

        if (!Provisioning_ControlBlock.isBleProvisioningActive)
        {
            sem_post(&AP_connectionAsyncSem);
        }
    }
        break;

    case SL_WLAN_EVENT_DISCONNECT:
    {
        if (Provisioning_ControlBlock.isBleProvisioningActive)
        {
            Display_printf(
                    displayOut, 0, 0,
                    " [Event] Station disconnected from AP (Reason Code = %d)",
                    pWlanEvent->Data.Disconnect.ReasonCode);

            Provisioning_ControlBlock.resolvedIP = 0;
            Provisioning_ControlBlock.gatewayIP = 0;
        }
        else
        {
            SlWlanEventDisconnect_t* pEventData = NULL;

            CLR_STATUS_BIT(Provisioning_ControlBlock.status,
                           AppStatusBits_Connection);
            CLR_STATUS_BIT(Provisioning_ControlBlock.status,
                           AppStatusBits_IpAcquired);
            CLR_STATUS_BIT(Provisioning_ControlBlock.status,
                           AppStatusBits_Ipv6lAcquired);
            CLR_STATUS_BIT(Provisioning_ControlBlock.status,
                           AppStatusBits_Ipv6gAcquired);

            pEventData = &pWlanEvent->Data.Disconnect;

            /*  If the user has initiated 'Disconnect' request,
             * 'reason_code' is SL_WLAN_DISCONNECT_USER_INITIATED.
             */
            if (SL_WLAN_DISCONNECT_USER_INITIATED == pEventData->ReasonCode)
            {
                Display_printf(displayOut, 0, 0,
                               "[WLAN EVENT]Device disconnected from the "
                               "AP: %s, BSSID: %x:%x:%x:%x:%x:%x "
                               "on application's request",
                               Provisioning_ControlBlock.connectionSSID,
                               Provisioning_ControlBlock.connectionBSSID[0],
                               Provisioning_ControlBlock.connectionBSSID[1],
                               Provisioning_ControlBlock.connectionBSSID[2],
                               Provisioning_ControlBlock.connectionBSSID[3],
                               Provisioning_ControlBlock.connectionBSSID[4],
                               Provisioning_ControlBlock.connectionBSSID[5]);
            }
            else
            {
                Display_printf(
                        displayOut, 0, 0,
                        "[WLAN ERROR]Device disconnected from the AP AP: %s,"
                        "BSSID: %x:%x:%x:%x:%x:%x on an ERROR..!!",
                        Provisioning_ControlBlock.connectionSSID,
                        Provisioning_ControlBlock.connectionBSSID[0],
                        Provisioning_ControlBlock.connectionBSSID[1],
                        Provisioning_ControlBlock.connectionBSSID[2],
                        Provisioning_ControlBlock.connectionBSSID[3],
                        Provisioning_ControlBlock.connectionBSSID[4],
                        Provisioning_ControlBlock.connectionBSSID[5]);
            }
            memset(Provisioning_ControlBlock.connectionSSID, 0,
                   sizeof(Provisioning_ControlBlock.connectionSSID));
            memset(Provisioning_ControlBlock.connectionBSSID, 0,
                   sizeof(Provisioning_ControlBlock.connectionBSSID));
        }
    }
        break;

    case SL_WLAN_EVENT_STA_ADDED:
    {
        Display_printf(
                displayOut,
                0,
                0,
                " [Event] New STA Added (MAC Address: %.2x:%.2x:%.2x:%.2x:%.2x)",
                pWlanEvent->Data.STAAdded.Mac[0],
                pWlanEvent->Data.STAAdded.Mac[1],
                pWlanEvent->Data.STAAdded.Mac[2],
                pWlanEvent->Data.STAAdded.Mac[3],
                pWlanEvent->Data.STAAdded.Mac[4],
                pWlanEvent->Data.STAAdded.Mac[5]);
    }
        break;

    case SL_WLAN_EVENT_STA_REMOVED:
    {
        Display_printf(
                displayOut,
                0,
                0,
                " [Event] New STA Removed (MAC Address: %.2x:%.2x:%.2x:%.2x:%.2x)",
                pWlanEvent->Data.STAAdded.Mac[0],
                pWlanEvent->Data.STAAdded.Mac[1],
                pWlanEvent->Data.STAAdded.Mac[2],
                pWlanEvent->Data.STAAdded.Mac[3],
                pWlanEvent->Data.STAAdded.Mac[4],
                pWlanEvent->Data.STAAdded.Mac[5]);
    }
        break;

    case SL_WLAN_EVENT_PROVISIONING_PROFILE_ADDED:
    {
        Display_printf(displayOut, 0, 0,
                       "[Provisioning] Profile Added: SSID: %s",
                       pWlanEvent->Data.ProvisioningProfileAdded.Ssid);
    }
        break;

    case SL_WLAN_EVENT_PROVISIONING_STATUS:
    {
        switch (pWlanEvent->Data.ProvisioningStatus.ProvisioningStatus)
        {
        case SL_WLAN_PROVISIONING_GENERAL_ERROR:
        case SL_WLAN_PROVISIONING_ERROR_ABORT:
        case SL_WLAN_PROVISIONING_ERROR_ABORT_INVALID_PARAM:
        case SL_WLAN_PROVISIONING_ERROR_ABORT_HTTP_SERVER_DISABLED:
        case SL_WLAN_PROVISIONING_ERROR_ABORT_PROFILE_LIST_FULL:
        case SL_WLAN_PROVISIONING_ERROR_ABORT_PROVISIONING_ALREADY_STARTED:
        {
            Display_printf(
                    displayOut, 0, 0,
                    " [Provisioning] Provisioning Error status=%d",
                    pWlanEvent->Data.ProvisioningStatus.ProvisioningStatus);

        }
            break;

        case SL_WLAN_PROVISIONING_CONFIRMATION_STATUS_FAIL_NETWORK_NOT_FOUND:
        {
            Display_printf(
                    displayOut,
                    0,
                    0,
                    " [Provisioning] Profile confirmation failed: "
                    "network not found");
        }
            break;

        case SL_WLAN_PROVISIONING_CONFIRMATION_STATUS_FAIL_CONNECTION_FAILED:
        {
            Display_printf(
                    displayOut,
                    0,
                    0,
                    " [Provisioning] Profile confirmation failed: "
                    "Connection failed");
        }
            break;

        case SL_WLAN_PROVISIONING_CONFIRMATION_STATUS_CONNECTION_SUCCESS_IP_NOT_ACQUIRED:
        {
            Display_printf(
                    displayOut,
                    0,
                    0,
                    " [Provisioning] Profile confirmation failed: "
                    "IP address not acquired");
        }
            break;

        case SL_WLAN_PROVISIONING_CONFIRMATION_STATUS_SUCCESS_FEEDBACK_FAILED:
        {
            Display_printf(
                    displayOut,
                    0,
                    0,
                    " [Provisioning] Profile Confirmation failed "
                    "(Connection Success, feedback to Smartphone app failed)");
        }
            break;

        case SL_WLAN_PROVISIONING_CONFIRMATION_STATUS_SUCCESS:
        {
            Display_printf(displayOut, 0, 0,
                           " [Provisioning] Profile Confirmation Success!");
            if (!Provisioning_ControlBlock.isBleProvisioningActive)
            {
                AP_signalProvisioningEvent(PrvnEvent_ConfirmationSuccess);
            }
        }
            break;

        case SL_WLAN_PROVISIONING_AUTO_STARTED:
        {
            Display_printf(displayOut, 0, 0,
                           " [Provisioning] Auto-Provisioning Started");
            if (!Provisioning_ControlBlock.isBleProvisioningActive)
            {
                AP_signalProvisioningEvent(PrvnEvent_Stopped);
            }
        }
            break;

        case SL_WLAN_PROVISIONING_STOPPED:
        {
            Display_printf(displayOut, 0, 0, "Provisioning stopped:");
            Display_printf(displayOut, 0, 0, " Current Role: %s",
                           Roles[pWlanEvent->Data.ProvisioningStatus.Role]);

            if (ROLE_STA == pWlanEvent->Data.ProvisioningStatus.Role)
            {
                Display_printf(
                        displayOut,
                        0,
                        0,
                        "WLAN Status: %s",
                        WlanStatus[pWlanEvent->Data.ProvisioningStatus.WlanStatus]);

                if (SL_WLAN_STATUS_CONNECTED
                        == pWlanEvent->Data.ProvisioningStatus.WlanStatus)
                {
                    Display_printf(displayOut, 0, 0,
                                   " [WLAN EVENT] - Connected to SSID:%s",
                                   pWlanEvent->Data.ProvisioningStatus.Ssid);
                    if (!Provisioning_ControlBlock.isBleProvisioningActive)
                    {
                        memcpy(Provisioning_ControlBlock.connectionSSID,
                               pWlanEvent->Data.ProvisioningStatus.Ssid,
                               pWlanEvent->Data.ProvisioningStatus.Ssidlen);
                        Provisioning_ControlBlock.ssidLen =
                                pWlanEvent->Data.ProvisioningStatus.Ssidlen;

                        /* Provisioning is stopped by the device and provisioning
                         * is done successfully */
                        AP_signalProvisioningEvent(PrvnEvent_Stopped);

                        break;
                    }
                }
                else
                {
                    if (!Provisioning_ControlBlock.isBleProvisioningActive)
                    {
                        CLR_STATUS_BIT(Provisioning_ControlBlock.status,
                                       AppStatusBits_Connection);
                        CLR_STATUS_BIT(Provisioning_ControlBlock.status,
                                       AppStatusBits_IpAcquired);
                        CLR_STATUS_BIT(Provisioning_ControlBlock.status,
                                       AppStatusBits_Ipv6lAcquired);
                        CLR_STATUS_BIT(Provisioning_ControlBlock.status,
                                       AppStatusBits_Ipv6gAcquired);

                        /* Provisioning is stopped by the device and provisioning is
                         * not done yet, still need to connect to AP */
                        AP_signalProvisioningEvent(PrvnEvent_WaitForConn);

                        break;
                    }
                }
            }
            AP_signalProvisioningEvent(PrvnEvent_Stopped);

        }
            break;

        case SL_WLAN_PROVISIONING_SMART_CONFIG_SYNCED:
        {
            Display_printf(displayOut, 0, 0,
                           " [Provisioning] Smart Config Synced!");
        }
            break;

        case SL_WLAN_PROVISIONING_SMART_CONFIG_SYNC_TIMEOUT:
        {
            Display_printf(displayOut, 0, 0,
                           " [Provisioning] Smart Config Sync Timeout!");
        }
            break;

        case SL_WLAN_PROVISIONING_CONFIRMATION_WLAN_CONNECT:
        {
            Display_printf(
                    displayOut, 0, 0,
                    " [Provisioning] Profile confirmation: WLAN Connected!");

            if (!Provisioning_ControlBlock.isBleProvisioningActive)
            {
                SET_STATUS_BIT(Provisioning_ControlBlock.status,
                               AppStatusBits_Connection);
                CLR_STATUS_BIT(Provisioning_ControlBlock.status,
                               AppStatusBits_IpAcquired);
                CLR_STATUS_BIT(Provisioning_ControlBlock.status,
                               AppStatusBits_Ipv6lAcquired);
                CLR_STATUS_BIT(Provisioning_ControlBlock.status,
                               AppStatusBits_Ipv6gAcquired);

                Display_printf(displayOut, 0, 0,
                               "[WLAN EVENT] Connection to AP succeeded");
            }
        }
            break;

        case SL_WLAN_PROVISIONING_CONFIRMATION_IP_ACQUIRED:
        {
            Display_printf(
                    displayOut, 0, 0,
                    " [Provisioning] Profile confirmation: IP Acquired!");
        }
            break;

        case SL_WLAN_PROVISIONING_EXTERNAL_CONFIGURATION_READY:
        {
            Display_printf(displayOut, 0, 0,
                           " [Provisioning] External configuration is ready!");
        }
            break;

        default:
        {
            Display_printf(
                    displayOut, 0, 0,
                    " [Provisioning] Unknown Provisioning Status: %d",
                    pWlanEvent->Data.ProvisioningStatus.ProvisioningStatus);
        }
            break;

        }
    }
        break;
    }
}

//*****************************************************************************
//
//! \brief The Function Handles the Fatal errors
//!
//! \param[in]  slFatalErrorEvent - Pointer to Fatal Error Event info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkFatalErrorEventHandler(SlDeviceFatal_t *slFatalErrorEvent)
{
    switch (slFatalErrorEvent->Id)
    {
    case SL_DEVICE_EVENT_FATAL_DEVICE_ABORT:
    {
        Display_printf(displayOut, 0, 0,
                       "[ERROR] - FATAL ERROR: Abort NWP event detected: "
                       "AbortType=%d, AbortData=0x%x",
                       slFatalErrorEvent->Data.DeviceAssert.Code,
                       slFatalErrorEvent->Data.DeviceAssert.Value);
    }
        break;

    case SL_DEVICE_EVENT_FATAL_DRIVER_ABORT:
    {
        Display_printf(displayOut, 0, 0,
                       "[ERROR] - FATAL ERROR: Driver Abort detected.");
    }
        break;

    case SL_DEVICE_EVENT_FATAL_NO_CMD_ACK:
    {
        Display_printf(displayOut, 0, 0,
                       "[ERROR] - FATAL ERROR: No Cmd Ack detected "
                       "[cmd opcode = 0x%x]",
                       slFatalErrorEvent->Data.NoCmdAck.Code);
    }
        break;

    case SL_DEVICE_EVENT_FATAL_SYNC_LOSS:
    {
        Display_printf(displayOut, 0, 0,
                       "[ERROR] - FATAL ERROR: Sync loss detected");
    }
        break;

    default:
    {
        Display_printf(displayOut, 0, 0,
                       "[ERROR] - FATAL ERROR: Unspecified error detected");
    }
        break;
    }
}

//*****************************************************************************
//
//! \brief This function handles network events such as IP acquisition, IP
//!           leased, IP released etc.
//!
//! \param[in]  pNetAppEvent - Pointer to NetApp Event Info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkNetAppEventHandler(SlNetAppEvent_t *pNetAppEvent)
{
    uint32_t eventPend;

    switch (pNetAppEvent->Id)
    {
    case SL_NETAPP_EVENT_IPV4_ACQUIRED:
    {
        sprintf((char*) resolvedIPString, "%d.%d.%d.%d",
                SL_IPV4_BYTE(pNetAppEvent->Data.IpAcquiredV4.Ip, 3),
                SL_IPV4_BYTE(pNetAppEvent->Data.IpAcquiredV4.Ip, 2),
                SL_IPV4_BYTE(pNetAppEvent->Data.IpAcquiredV4.Ip, 1),
                SL_IPV4_BYTE(pNetAppEvent->Data.IpAcquiredV4.Ip, 0));
        Provisioning_ControlBlock.resolvedIP = pNetAppEvent->Data.IpAcquiredV4.Ip;
        Display_printf(displayOut, 0, 0, "[NETAPP EVENT] IP Acquired: IP=%s",
                       resolvedIPString);

        sprintf((char*) gatewayIPString, "%d.%d.%d.%d",
                SL_IPV4_BYTE(pNetAppEvent->Data.IpAcquiredV4.Gateway, 3),
                SL_IPV4_BYTE(pNetAppEvent->Data.IpAcquiredV4.Gateway, 2),
                SL_IPV4_BYTE(pNetAppEvent->Data.IpAcquiredV4.Gateway, 1),
                SL_IPV4_BYTE(pNetAppEvent->Data.IpAcquiredV4.Gateway, 0));
        Provisioning_ControlBlock.gatewayIP = pNetAppEvent->Data.IpAcquiredV4.Gateway;
        Display_printf(displayOut, 0, 0, "[NETAPP EVENT] Gateway: %s",
                       gatewayIPString);

        if (Provisioning_ControlBlock.isBleProvisioningActive)
        {
            /* Send a message to the WiFi thread  */
            eventPend = WIFI_EVT_SL_CONNECTED;
            mq_send(wifiQueueSend, (void*) &eventPend, sizeof(uint32_t), 1);
        }
        else
        {
            sem_post(&AP_connectionAsyncSem);
        }
    }
        break;

    case SL_NETAPP_EVENT_IPV6_ACQUIRED:
    {
        if (!Provisioning_ControlBlock.isBleProvisioningActive)
        {
            if (!GET_STATUS_BIT(Provisioning_ControlBlock.status,
                                AppStatusBits_Ipv6lAcquired))
            {
                SET_STATUS_BIT(Provisioning_ControlBlock.status,
                               AppStatusBits_Ipv6lAcquired);
                Display_printf(displayOut, 0, 0,
                               "[NETAPP EVENT] Local IPv6 Acquired");
            }
            else
            {
                SET_STATUS_BIT(Provisioning_ControlBlock.status,
                               AppStatusBits_Ipv6gAcquired);
                Display_printf(displayOut, 0, 0,
                               "[NETAPP EVENT] Global IPv6 Acquired");
            }

            sem_post(&AP_connectionAsyncSem);
        }
    }
        break;

    case SL_NETAPP_EVENT_DHCPV4_LEASED:
    {
        Display_printf(displayOut, 0, 0,
                       "[NETAPP Event] IP Leased: %d.%d.%d.%d",
                       SL_IPV4_BYTE(pNetAppEvent->Data.IpLeased.IpAddress, 3),
                       SL_IPV4_BYTE(pNetAppEvent->Data.IpLeased.IpAddress, 2),
                       SL_IPV4_BYTE(pNetAppEvent->Data.IpLeased.IpAddress, 1),
                       SL_IPV4_BYTE(pNetAppEvent->Data.IpLeased.IpAddress, 0));
        if (Provisioning_ControlBlock.isBleProvisioningActive)
        {
            eventPend = WIFI_EVT_SL_CONNECTED;
            mq_send(wifiQueueSend, (void*) &eventPend, sizeof(uint32_t), 1);
        }
        else
        {
            SET_STATUS_BIT(Provisioning_ControlBlock.status, AppStatusBits_IpLeased);
        }
    }
        break;

    case SL_NETAPP_EVENT_DHCP_IPV4_ACQUIRE_TIMEOUT:
    {
        Display_printf(displayOut, 0, 0,
                       "[NETAPP Event] IP acquisition timeout");
        break;
    }
    default:
    {
        Display_printf(displayOut, 0, 0,
                       "[NETAPP EVENT] Unhandled event [0x%x]",
                       pNetAppEvent->Id);
    }
        break;
    }
}

//*****************************************************************************
//
//! \brief This function handles HTTP server events
//!
//! \param[in]  pServerEvent    - Contains the relevant event information
//! \param[in]  pServerResponse - Should be filled by the user with the
//!                               relevant response information
//!
//! \return None
//!
//****************************************************************************
void SimpleLinkHttpServerEventHandler(
        SlNetAppHttpServerEvent_t *pHttpEvent,
        SlNetAppHttpServerResponse_t *pHttpResponse)
{
    /* Unused in this application */
}

//*****************************************************************************
//
//! \brief This function handles General Events
//!
//! \param[in]  pDevEvent - Pointer to General Event Info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkGeneralEventHandler(SlDeviceEvent_t *pDevEvent)
{
    /* Most of the general errors are not FATAL are are to be handled
     appropriately by the application */
    Display_printf(displayOut, 0, 0, "[GENERAL EVENT] - ID=[%d] Sender=[%d]",
                   pDevEvent->Data.Error.Code, pDevEvent->Data.Error.Source);
}

//*****************************************************************************
//
//! \brief This function handles socket events indication
//!
//! \param[in]  pSock - Pointer to Socket Event Info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkSockEventHandler(SlSockEvent_t *pSock)
{
    /* This application doesn't work with socket - Events are not expected */
    switch (pSock->Event)
    {
    case SL_SOCKET_TX_FAILED_EVENT:
        switch (pSock->SocketAsyncEvent.SockTxFailData.Status)
        {
        case SL_ERROR_BSD_ECLOSE:
            Display_printf(displayOut, 0, 0,
                           "[SOCK ERROR] - close socket (%d) operation "
                           "failed to transmit all queued packets",
                           pSock->SocketAsyncEvent.SockTxFailData.Sd);
            break;
        default:
            Display_printf(displayOut, 0, 0,
                           "[SOCK ERROR] - TX FAILED  :  socket %d , "
                           "reason (%d)",
                           pSock->SocketAsyncEvent.SockTxFailData.Sd,
                           pSock->SocketAsyncEvent.SockTxFailData.Status);
            break;
        }
        break;

    default:
        Display_printf(displayOut, 0, 0,
                       "[SOCK EVENT] - Unexpected Event [%x0x]", pSock->Event);
        break;
    }
}

void SimpleLinkNetAppRequestEventHandler(SlNetAppRequest_t *pNetAppRequest,
                                         SlNetAppResponse_t *pNetAppResponse)
{
    /* Unused in this application */
}

void SimpleLinkNetAppRequestMemFreeEventHandler(uint8_t *buffer)
{
    /* Unused in this application */
}

/*******************************************************************************
 * @fn      AP_initServices
 *
 * @brief   Configure SNP and register services.
 *
 * @param   None.
 *
 * @return  None.
 ******************************************************************************/
static void BLE_initServices(void)
{
    uint8_t statusValue = 0;
    uint8_t blankString[] = "";

    BLE_WiFi_Profile_setParameter(BLE_WIFI_PROV_STATUS_ID, sizeof(uint8_t),
                                  &statusValue);
    BLE_WiFi_Profile_setParameter(BLE_WIFI_PROV_NOTIFY_ID, sizeof(uint8_t),
                                  &statusValue);
    BLE_WiFi_Profile_setParameter(BLE_WIFI_PROV_START_ID, sizeof(uint8_t),
                                  &statusValue);
    BLE_WiFi_Profile_setParameter(BLE_WIFI_PROV_SSID_ID, 1, blankString);
    BLE_WiFi_Profile_setParameter(BLE_WIFI_PROV_PASS_ID, 1, blankString);
    BLE_WiFi_Profile_setParameter(BLE_WIFI_PROV_DEVNAME_ID, 1, blankString);

    /* Add the SimpleProfile Service to the SNP. */
    BLE_WiFi_Profile_AddService();
    SAP_registerEventCB(BLE_processSNPEventCB, 0xFFFF);
}

//*****************************************************************************
//
//! \brief  Main application thread
//!
//! \param  none
//!
//! \return none
//!
//*****************************************************************************
void * bleThread(void *arg)
{
    SAP_Params sapParams;

    struct timespec ts;
    unsigned int prio = 0;
    uint8_t enableAdv = 1;
    uint8_t disableAdv = 0;
    uint32_t eventPend;
    uint32_t curEvent = 0;
    ble_states_t curState = BLE_RESET;
    uint32_t disconnectReason = 0;
    int32_t sblRetCode;
    SBL_Params sblParams;

    /* Authentication parameters */
    uint32_t passkey;
    uint32_t tempPasskey;
    int numDigits = 0;
    uint8_t ioCap = SAP_DISPLAY_ONLY;
    uint8_t pairMode = SAP_SECURITY_WAIT_FOR_REQUEST;
    uint8_t bonding = 1;

    /* Register key handler and setup timer*/
    /* Setting up the timer in single shot mode for 3s */
    Timer_Params_init(&timerParams);
    timerParams.period = 3000000;
    timerParams.periodUnits = Timer_PERIOD_US;
    timerParams.timerMode = Timer_ONESHOT_CALLBACK;
    timerParams.timerCallback = BLE_timerHandler;

    buttonTimer = Timer_open(Board_TIMER0, &timerParams);

    if (buttonTimer == NULL)
    {
        Display_printf(displayOut, 0, 0,
                       "[bleThread] Failed to initialize Timer!");
    }

    GPIO_setCallback(Board_GPIO_BUTTON1, (GPIO_CallbackFxn) BLE_keyHandler);
    GPIO_enableInt(Board_GPIO_BUTTON1);

    /* Register to receive notifications from Simple Profile if characteristics
     have been written to */
    BLE_WiFi_Profile_RegisterAppCB(&bleCallbacks);

    /* Displaying Banner */
    Display_printf(displayOut, 0, 0,
                   "===================================================");
    Display_printf(displayOut, 0, 0, " BLE Provisioning Example Version %s",
    APPLICATION_VERSION);
    Display_printf(displayOut, 0, 0,
                   "===================================================");

    while (1)
    {
        switch (curState)
        {
        case BLE_RESET:
        {
            /* Make sure CC26xx is not in BSL and restarting it */
            GPIO_write(Board_RESET, 0);
            GPIO_write(Board_SRDY, 1);

            usleep(10000);

            GPIO_write(Board_RESET, 1);

            Display_printf(
                    displayOut, 0, 0,
                    "[bleThread] Initializing CC26xx BLE network processor...");

            /* Initialize UART port parameters within SAP parameters */
            SAP_initParams(SAP_PORT_REMOTE_UART, &sapParams);

            sapParams.port.remote.mrdyPinID = Board_MRDY;
            sapParams.port.remote.srdyPinID = Board_SRDY;
            sapParams.port.remote.boardID = Board_UART0;

            /* Setup NP module */
            SAP_open(&sapParams);

            /* Register Application thread's callback to receive
             asynchronous requests from the NP. */
            SAP_setAsyncCB(BLE_asyncCB);

            /* Reset the NP, and await a power-up indication.
             * Clear any pending power indications received prior to this
             * reset call
             */
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += 1;
            mq_timedreceive(bleQueueRec, (void*) &curEvent, sizeof(uint32_t),
                            &prio, &ts);

            SAP_reset();

            curEvent = 0;
            GPIO_enableInt(Board_GPIO_BUTTON1);
            mq_receive(bleQueueRec, (void*) &curEvent, sizeof(uint32_t), &prio);
            GPIO_disableInt(Board_GPIO_BUTTON1);

            if (curEvent == BLE_EVT_BSL_BUTTON)
            {
                curState = BLE_SBL;
            }
            else if (curEvent == BLE_EVT_PUI)
            {
                Display_printf(displayOut, 0, 0,
                               "[bleThread] Initialized BLE SNP!");

                /* Read BD ADDR */
                SAP_setParam(SAP_PARAM_HCI, SNP_HCI_OPCODE_READ_BDADDR, 0,
                NULL);

                /* Setup Services - Service creation is blocking so no need
                 * to pend */
                BLE_initServices();
                curState = BLE_IDLE;
                Provisioning_ControlBlock.isBleProvisioningActive = true;

                /* Sending signal to Wi-Fi thread that the BLE SNP started */
                eventPend = WIFI_EVT_BLE_INITI;
                mq_send(wifiQueueSend, (void*) &eventPend, sizeof(uint32_t), 1);
            }
            else
            {
                Display_printf(displayOut, 0, 0,
                               "[bleThread] Warning! Unexpected Event %lu",
                               eventPend);
                continue;
            }
        }
            break;
        case BLE_START_ADV:
        {
            /* Turn on user LED to indicate advertising */
            GPIO_write(Board_LED0, Board_LED_ON);

            Display_printf(displayOut, 0, 0,
                           "[bleThread] Starting advertisement... ");

            /* Setting Advertising Name */
            SAP_setServiceParam(SNP_GGS_SERV_ID, SNP_GGS_DEVICE_NAME_ATT,
                                sizeof(snpDeviceName), snpDeviceName);

            /* Set advertising data. */
            SAP_setParam(SAP_PARAM_ADV, SAP_ADV_DATA_NOTCONN,
                         sizeof(advertData), advertData);

            /* Set scan response data. */
            SAP_setParam(SAP_PARAM_ADV, SAP_ADV_DATA_SCANRSP,
                         sizeof(scanRspData), scanRspData);

            /* Enable Advertising and await NP response */
            SAP_setParam(SAP_PARAM_ADV, SAP_ADV_STATE, 1, &enableAdv);

            do
            {
                curEvent = 0;
                mq_receive(bleQueueRec, (void*) &curEvent, sizeof(uint32_t),
                           &prio);

                if (curEvent != BLE_EVT_ADV_ENB)
                {
                    Display_printf(displayOut, 0, 0,
                                   "[bleThread] Warning! Unexpected Event %lu",
                                   curEvent);
                }
            }
            while (curEvent != BLE_EVT_ADV_ENB);

            Display_printf(displayOut, 0, 0,
                           "[bleThread] Advertisement started!");
            Display_printf(
                    displayOut, 0, 0,
                    "[bleThread] Waiting for connection (or timeout)... ");

            /* Wait for connection or button press to cancel advertisement */
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += 30;
            curEvent = 0;
            mq_timedreceive(bleQueueRec, (void*) &curEvent, sizeof(uint32_t),
                            &prio, &ts);

            Display_printf(displayOut, 0, 0,
                           "[bleThread] BLE Start Advertisement. Event %lu",
                           curEvent);

            if (curEvent == BLE_EVT_CONN_EST)
            {
                GPIO_disableInt(Board_GPIO_BUTTON1);
                curState = BLE_CONNECTED;
            }
            else if (curEvent == BLE_EVT_BSL_BUTTON)
            {
                curState = BLE_SBL;
                break;
            }
            else if (curEvent == BLE_EVT_SWITCH_PROV)
            {
                Display_printf(
                        displayOut, 0, 0,
                        "[bleThread] Switching to AP provisioning mode!");
                curState = BLE_CANCEL_ADV;
                break;
            }
            else
            {
                curState = BLE_CANCEL_ADV;
                Display_printf(displayOut, 0, 0,
                               "[bleThread] Advertisement Timeout!");

                /* Sending signal to Wi-Fi thread that we timed out */
                eventPend = WIFI_EVT_BLE_TIME;
                mq_send(wifiQueueSend, (void*) &eventPend, sizeof(uint32_t), 1);
            }
        }
            break;

        case BLE_CONNECTED:
        {
            /* Before connecting, NP will send the stop ADV message */
            do
            {
                curEvent = 0;
                mq_receive(bleQueueRec, (void*) &curEvent, sizeof(uint32_t),
                           &prio);

                if (curEvent != BLE_EVT_ADV_END)
                {
                    Display_printf(displayOut, 0, 0,
                                   "[bleThread] Warning! Unexpected Event %lu",
                                   eventPend);
                }
            }
            while (curEvent != BLE_EVT_ADV_END);

            /* Update State and Characteristic values on LCD */
            Display_printf(displayOut, 0, 0,
                           "[bleThread]  Peer connected! (%s)", peerstr);

            /* Set IO capabilities. This device only has display capabilities,
             * so it will display the passkey during pairing */
            SAP_setParam(SAP_PARAM_SECURITY, SAP_SECURITY_IOCAPS, 1, &ioCap);

            /* Set pairing mode to wait for a request to pair */
            SAP_setParam(SAP_PARAM_SECURITY, SAP_SECURITY_BEHAVIOR, 1,
                         &pairMode);

            /* Request bonding so long-term keys for re-encryption upon
             * subsequent connections without repairing */
            SAP_setParam(SAP_PARAM_SECURITY, SAP_SECURITY_BONDING, 1, &bonding);

            /* Send security parameters to central device */
            SAP_sendSecurityRequest();

            /* Wait to receive security event from central device indicating if
             * peripheral needs to pair or is already paired */
            do
            {
                curEvent = 0;
                mq_receive(bleQueueRec, (void*) &curEvent, sizeof(uint32_t),
                           &prio);

                if (curEvent != BLE_EVT_START_PAIR && curEvent != BLE_EVT_PAIRED)
                {
                    Display_printf(displayOut, 0, 0,
                                   "[bleThread] Warning! Unexpected Event %lu",
                                   eventPend);
                }
            }
            while (curEvent != BLE_EVT_START_PAIR && curEvent != BLE_EVT_PAIRED);

            /* Peripheral is not bonded, so start pairing process */
            if (curEvent == BLE_EVT_START_PAIR)
            {
                /* Generate random passkey */
                passkey = SAP_getRand() % 1000000;
                tempPasskey = passkey;
                numDigits = 0;

                /* Determine number of digits of the passkey and pad with zeros
                 * if less than 6 digits */
                while (tempPasskey != 0) {
                    tempPasskey /= 10;
                    numDigits++;
                }

                while (numDigits < 6)
                {
                    passkey *= 10;
                    numDigits++;
                }

                /* Set the passkey that must be entered on the central
                 * device for authentication  */
                SAP_setAuthenticationRsp(passkey);

                /* Display passkey to user */
                Display_printf(
                        displayOut, 0, 0,
                        "[bleThread] Please enter the passkey on your device:");
                Display_printf(displayOut, 0, 0, "[bleThread] %lu", passkey);

                /* Wait to receive event from central device indicating
                 * if pairing was a success */
                curEvent = 0;
                mq_receive(bleQueueRec, (void*) &curEvent, sizeof(uint32_t),
                           &prio);

                if (curEvent == BLE_EVT_PAIRED)
                {
                    curState = BLE_PAIRED;
                    Display_printf(
                            displayOut, 0, 0,
                            "[bleThread] Device is successfully paired!");
                }
                else if (curEvent == BLE_EVT_WRONG_PASSKEY)
                {
                    /* Disconnect the device and restart advertising */
                    SAP_setParam(SAP_PARAM_CONN, SAP_CONN_STATE,
                                 sizeof(connHandle), (uint8_t *) &connHandle);
                    curState = BLE_START_ADV;
                    Display_printf(
                            displayOut,
                            0,
                            0,
                            "[bleThread] Wrong passkey entered. "
                            "Reconnect and try again.");
                }
                else if (curEvent == BLE_EVT_CONN_TERM)
                {
                    /* Client has disconnected from server */
                    SAP_setParam(SAP_PARAM_CONN, SAP_CONN_STATE,
                                 sizeof(connHandle), (uint8_t *) &connHandle);
                    curState = BLE_START_ADV;
                    Display_printf(displayOut, 0, 0,
                                   "[bleThread] Disconnected from device.");
                }
                else
                {
                    Display_printf(displayOut, 0, 0,
                                   "[bleThread] Warning! Unexpected Event %lu",
                                   eventPend);
                }

            }
            else if (curEvent == BLE_EVT_PAIRED)
            {
                /* Peripheral is already bonded */
                Display_printf(displayOut, 0, 0,
                               "[bleThread] Device is successfully paired!");
                curState = BLE_PAIRED;
            }

        }
            break;

        case BLE_PAIRED:
        {
            curEvent = 0;
            mq_receive(bleQueueRec, (void*) &curEvent, sizeof(uint32_t), &prio);

            if (curEvent == BLE_EVT_START_PROVISION)
            {
                disconnectReason = WIFI_EVT_BLE_PROV_START;
            }
            else if (curEvent == BLE_EVT_PROV_COMPLETE)
            {
                disconnectReason = WIFI_EVT_BLE_PROV_COMPLETE;
            }
            else
            {
                disconnectReason = WIFI_EVT_BLE_TIME;
            }

            /* Client has disconnected from server */
            SAP_setParam(SAP_PARAM_CONN, SAP_CONN_STATE, sizeof(connHandle),
                         (uint8_t *) &connHandle);

            do
            {
                curEvent = 0;
                mq_receive(bleQueueRec, (void*) &curEvent, sizeof(uint32_t),
                           &prio);

                if (curEvent != BLE_EVT_CONN_TERM)
                {
                    Display_printf(displayOut, 0, 0,
                                   "[bleThread] Warning! Unexpected Event %lu",
                                   eventPend);
                }
            }
            while (curEvent != BLE_EVT_CONN_TERM);

            curState = BLE_CANCEL_ADV;
        }
            break;

        case BLE_CANCEL_ADV:
        {
            Display_printf(displayOut, 0, 0,
                           "[bleThread] Advertisement has been canceled!");

            /* Cancel Advertisement */
            SAP_setParam(SAP_PARAM_ADV, SAP_ADV_STATE, 1, &disableAdv);

            do
            {
                curEvent = 0;
                mq_receive(bleQueueRec, (void*) &curEvent, sizeof(uint32_t),
                           &prio);

            }
            while (curEvent != BLE_EVT_ADV_END);

            /* Checking to see if we have anything we want to communicate to
             * the Wi-Fi thread.
             */
            if (disconnectReason != 0)
            {
                eventPend = disconnectReason;
                mq_send(wifiQueueSend, (void*) &eventPend, sizeof(uint32_t), 1);
                disconnectReason = 0;
            }
            curState = BLE_IDLE;
        }
            break;

        case BLE_IDLE:
        {
            /* Turn off user LED to indicate stop advertising */
            GPIO_write(Board_LED0, Board_LED_OFF);
            GPIO_enableInt(Board_GPIO_BUTTON1);
            Display_printf(displayOut, 0, 0, "[bleThread] State set to idle.");

            if (switchProvisioningMode)
            {
                switchProvisioningMode = false;
                AP_signalProvisioningEvent(PrvnEvent_SwitchProv);
            }

            /* Key Press triggers state change from idle */
            mq_receive(bleQueueRec, (void*) &curEvent, sizeof(uint32_t), &prio);

            if (curEvent == BLE_EVT_BSL_BUTTON)
            {
                curState = BLE_SBL;
            }
            else if (curEvent == BLE_EVT_START_PROVISION)
            {
                curState = BLE_START_ADV;
            }
            else if (curEvent == BLE_EVT_SWITCH_PROV)
            {
                curState = BLE_START_ADV;
                switchProvisioningMode = false;
                AP_signalProvisioningEvent(PrvnEvent_SwitchProv);
            }
            else
            {
                curState = BLE_RESET;
            }
        }
            break;

        case BLE_SBL:
        {
            Display_printf(displayOut, 0, 0,
                           "[bleThread] Device being set into BSL mode.");

            /* Close NP so SBL can use serial port */
            SAP_close();

            GPIO_disableInt(Board_SRDY);
            /* Initialize SBL parameters and open port to target device */
            SBL_initParams(&sblParams);
            sblParams.resetPinID = Board_RESET;
            sblParams.blPinID = Board_MRDY;
            sblParams.targetInterface = SBL_DEV_INTERFACE_UART;
            sblParams.localInterfaceID = Board_UART0;

            Display_printf(displayOut, 0, 0,
                           "[bleThread] Programming the CC26xx... ");

            sblRetCode = WiFi_SNP_updateFirmware(&sblParams);

            if (sblRetCode != WIFI_SNP_SBL_SUCCESS)
            {
                Display_printf(
                        displayOut, 0, 0,
                        "[bleThread] ERROR: Cannot program SNP image (Code %d)",
                        sblRetCode);
            }
            else
            {
                Display_printf(
                        displayOut, 0, 0,
                        "[bleThread] SNP image programmed successfully.");
            }

            Display_printf(displayOut, 0, 0, "[bleThread] Resetting device.");

            /* Regardless of successful write we must restart the SNP
             force reset */
            PRCMMCUReset(true);
        }
            break;
        }
    }
}

void * wifiThread(void *arg)
{
    uint32_t curState = WIFI_INITIALIZE;
    struct mq_attr attr;
    uint8_t setValue;
    uint32_t curEvent = 0;
    sblUpdateInitiated = false;
    uint32_t eventPend = 0;
    unsigned int prio;
    int32_t retValue;
    pthread_attr_t pAttrs;
    struct sched_param priParam;
    pthread_t gSpawnThread;
    uint32_t failedPingCount;
    SlWlanSecParams_t securityParameters;
    uint8_t ssidName[BLE_WIFI_MAX_SSID_LEN];
    uint8_t passName[BLE_WIFI_MAX_PASS_LEN];
    uint8_t devName[BLE_WIFI_MAX_DEVNAME_LEN];

    /* Create RTOS Queue for BLE Provisioning */
    attr.mq_flags = 0;
    attr.mq_maxmsg = 64;
    attr.mq_msgsize = sizeof(uint32_t);
    attr.mq_curmsgs = 0;

    /* Initializing both Wi-Fi and BLE queues */
    bleQueueRec = mq_open("BLEProvision", O_RDWR | O_CREAT, 0664, &attr);
    bleQueueSend = mq_open("BLEProvision", O_RDWR | O_CREAT | O_NONBLOCK, 0664,
                           &attr);
    wifiQueueRec = mq_open("WIFIProvision", O_RDWR | O_CREAT, 0664, &attr);
    wifiQueueSend = mq_open("WIFIProvision", O_RDWR | O_CREAT | O_NONBLOCK,
                            0664, &attr);

    /* Creating the thread for sl_Task */
    pthread_attr_init(&pAttrs);
    priParam.sched_priority = SPAWN_TASK_PRIORITY;
    retValue = pthread_attr_setschedparam(&pAttrs, &priParam);
    retValue |= pthread_attr_setstacksize(&pAttrs, TASK_STACK_SIZE);
    retValue |= pthread_create(&gSpawnThread, &pAttrs, sl_Task, NULL);

    if (retValue)
    {
        Display_printf(displayOut, 0, 0,
                       "[wifiThread] ERROR: Could not spawn sl_Task");
        while (1)
            ;
    }

    while (1)
    {
        switch (curState)
        {
        case WIFI_INITIALIZE:
        {
            /* Waiting for the BLE driver to start up. Note that just because
             * the BLE driver starts does not mean that we are advertising.
             * Due to coexistence concerns we do not start advertising until
             * the Wi-Fi driver is stopped.
             */
            curEvent = 0;
            mq_receive(wifiQueueRec, (void*) &curEvent, sizeof(uint32_t),
                       &prio);

            if (curEvent != WIFI_EVT_BLE_INITI)
            {
                Display_printf(displayOut, 0, 0,
                               "[wifiThread] Warning! Unexpected Event %lu",
                               eventPend);
                continue;
            }

            Display_printf(displayOut, 0, 0,
                           "[wifiThread] Initializing the Wi-Fi NWP...");

            /* First we have to open the Wi-Fi NWP driver. If we acquire
             * an IP then we can step provisioning and go directly to
             * the ping loop.
             */
            if (WiFi_startConnection() == false)
            {
                Display_printf(
                        displayOut, 0, 0,
                        "[wifiThread] ERROR - Could not start Wi-Fi driver!");

                /* Send an error to the BLE thread and continuing */
                eventPend = BLE_EVT_ERROR;
                mq_send(bleQueueSend, (void*) &eventPend, sizeof(uint32_t), 1);
            }
            else if (!WiFi_obtainIPAddress())
            {
                Display_printf(
                        displayOut,
                        0,
                        0,
                        "[wifiThread] Could not obtain IP address, Provisioning.");
                sl_Stop(SL_STOP_TIMEOUT);
                curState = WIFI_IDLE;
            }
            else
            {
                /* We have an IP- start pinging */
                curState = WIFI_PING_LOOP;
            }
        }
            break;
        case WIFI_IDLE:
        {
            GPIO_enableInt(Board_GPIO_BUTTON1);

            /* Sending command over to BLE thread to start advertising */
            eventPend = BLE_EVT_START_PROVISION;
            mq_send(bleQueueSend, (void*) &eventPend, sizeof(uint32_t), 1);

            /* Waiting for the provisioning command to start */
            mq_receive(wifiQueueRec, (void*) &curEvent, sizeof(uint32_t),
                       &prio);

            if (curEvent == WIFI_EVT_BLE_PROV_START)
            {
                curState = WIFI_ATTEMPT_PROVISION;
            }
            else if (curEvent == WIFI_EVT_RESET)
            {
                curState = WIFI_INITIALIZE;
            }
            else
            {
                Display_printf(
                        displayOut,
                        0,
                        0,
                        "[wifiThread] BLE timed out. Trying to connect to Wi-Fi.");

                if (WiFi_startConnection() && WiFi_obtainIPAddress())
                {
                    Display_printf(
                            displayOut,
                            0,
                            0,
                            "[wifiThread] Network connected. Going to ping loop!");
                    curState = WIFI_PING_LOOP;
                }
                else
                {
                    Display_printf(
                            displayOut,
                            0,
                            0,
                            "[wifiThread] Still no valid Wi-Fi connection. "
                            "Provisioning.");
                    sl_Stop(SL_STOP_TIMEOUT);
                }
            }
        }
            break;
        case WIFI_PING_LOOP:
        {
            Display_printf(displayOut, 0, 0, "-------- PING LOOP --------");
            GPIO_enableInt(Board_GPIO_BUTTON1);
            failedPingCount = 0;
            while (Provisioning_ControlBlock.resolvedIP != 0)
            {
                if (WiFi_pingGateway(20) == false)
                {
                    failedPingCount++;
                }

                if (failedPingCount > 3)
                {
                    Display_printf(
                            displayOut,
                            0,
                            0,
                            "Ping failed four consecutive attempts. "
                            "Re-provisioning");
                    Provisioning_ControlBlock.resolvedIP = 0;
                }
            }
            GPIO_disableInt(Board_GPIO_BUTTON1);
            Display_printf(displayOut, 0, 0, "-------- PING LOOP END --------");

            /* If we reach this point we should stop the Wi-Fi driver and
             * restart BLE provisioning.
             */
            sl_Stop(SL_STOP_TIMEOUT);

            /* If the SBL button initiated the disconnect we need to send an
             * update to BLE thread and bail out of the thread. The BLE
             * thread will restart the MCU regardless if the SBL update
             * is successful or not.
             */
            if (sblUpdateInitiated == true)
            {
                sblUpdateInitiated = false;
                eventPend = BLE_EVT_BSL_BUTTON;
                mq_send(bleQueueSend, (void*) &eventPend, sizeof(uint32_t), 1);
                return NULL;
            }

            curState = WIFI_IDLE;
        }
            break;
        case WIFI_ATTEMPT_PROVISION:
        {
            /* Assume failure. If provisioning succeeds, set to PING_LOOP */
            curState = WIFI_IDLE;

            Display_printf(displayOut, 0, 0,
                           "[wifiThread] -------- PROVISION ATTEMPT --------");

            /* First we should open the WiFi driver  */
            if (WiFi_startConnection() == false)
            {
                Display_printf(displayOut, 0, 0,
                               "[wifiThread] Could not open WiFi Driver!");
                break;
            }


#ifdef DELETE_PROFILES
            /* First we want to delete all currently provisioned profiles */
            sl_WlanProfileDel(0xFF);
#endif

            /* Parsing out all of the relevant provisioning information from
             * the BLE driver
             */
            BLE_WiFi_Profile_getParameter(BLE_WIFI_PROV_SSID_ID, ssidName);
            BLE_WiFi_Profile_getParameter(BLE_WIFI_PROV_PASS_ID, passName);
            BLE_WiFi_Profile_getParameter(BLE_WIFI_PROV_DEVNAME_ID, devName);

            Display_printf(displayOut, 0, 0,
                           "[wifiThread] Connecting to SSID %s...", ssidName);

            /* Setting up the security parameters. If the security key has
             * length then we will try initially with WPA2 (and WEP as a
             * fall back if that fails).
             */
            if (strlen((const char*) passName) > 0)
            {
                securityParameters.Key = (void*) passName;
                securityParameters.KeyLen = strlen((const char *) passName);
                securityParameters.Type = SL_WLAN_SEC_TYPE_WPA_WPA2;
            }
            else
            {
                securityParameters.Key = NULL;
                securityParameters.KeyLen = 0;
                securityParameters.Type = SL_WLAN_SEC_TYPE_OPEN;
            }

            retValue = sl_WlanConnect((const int8_t*) ssidName,
                                        strlen((const char *) ssidName),
                                        NULL,
                                        &securityParameters, NULL);

            if (retValue < 0)
            {
                Display_printf(displayOut, 0, 0,
                               "[wifiThread] Cannot connect! (err: %d)",
                               retValue);
                setValue = BLE_WIFI_PROV_FAIL_IP;
                BLE_WiFi_Profile_setParameter(BLE_WIFI_PROV_STATUS_ID, 1,
                                              &setValue);
                break;
            }

            retValue = sl_WlanProfileAdd((const int8_t*) ssidName,
                                         strlen((const char *) ssidName),
                                         NULL,
                                         &securityParameters, NULL, 1, 0);

            if (retValue < 0)
            {
                Display_printf(displayOut, 0, 0,
                               "[wifiThread] Cannot add profile! (err: %d)",
                               retValue);
                setValue = BLE_WIFI_PROV_FAIL_IP;
                BLE_WiFi_Profile_setParameter(BLE_WIFI_PROV_STATUS_ID, 1,
                                              &setValue);
#ifdef DELETE_PROFILES
                sl_WlanProfileDel(0xFF);
#endif
                break;
            }

            /* Trying to obtain an IP address */
            if (WiFi_obtainIPAddress() == false)
            {
                /* If we were WPA before, trying again as WEP */
                if (securityParameters.Type == SL_WLAN_SEC_TYPE_WPA_WPA2)
                {
                    Display_printf(
                            displayOut,
                            0,
                            0,
                            "[wifiThread] Connection failed. Retrying with WEP.");

#ifdef DELETE_PROFILES
                    sl_WlanProfileDel(0xFF);
#endif
                    securityParameters.Type = SL_WLAN_SEC_TYPE_WEP;

                    retValue = sl_WlanConnect((const int8_t*) ssidName,
                            strlen((const char *) ssidName),
                            NULL,
                            &securityParameters, NULL);

                    if (retValue < 0)
                    {
                        Display_printf(
                                displayOut, 0, 0,
                                "[wifiThread] Cannot connect! (err: %d)",
                                retValue);
                        setValue = BLE_WIFI_PROV_FAIL_IP;
                        BLE_WiFi_Profile_setParameter(BLE_WIFI_PROV_STATUS_ID,
                                                      1, &setValue);
                        break;
                    }

                    retValue = sl_WlanProfileAdd(
                            (const int8_t*) ssidName,
                            strlen((const char *) ssidName),
                            NULL,
                            &securityParameters, NULL, 1, 0);

                    if (retValue < 0)
                    {
                        Display_printf(
                                displayOut, 0, 0,
                                "[wifiThread] Cannot add profile! (err: %d)",
                                retValue);
                        setValue = BLE_WIFI_PROV_FAIL_IP;
                        BLE_WiFi_Profile_setParameter(BLE_WIFI_PROV_STATUS_ID,
                                                      1, &setValue);
#ifdef DELETE_PROFILES
                        sl_WlanProfileDel(0xFF);
#endif
                        break;
                    }

                    if (WiFi_obtainIPAddress() == true)
                    {
                        goto ProvisionIPAssigned;
                    }
                }
                setValue = BLE_WIFI_PROV_FAIL_IP;
                BLE_WiFi_Profile_setParameter(BLE_WIFI_PROV_STATUS_ID, 1,
                                              &setValue);
                Display_printf(displayOut, 0, 0,
                               "[wifiThread] ERROR - Cannot obtain IP address!",
                               retValue);
            }

            ProvisionIPAssigned:
            /* Finally trying to ping the gateway. If the ping succeeds it means
             * that we have successfully provisioned the device and relay this
             * back to the BLE host device.
             */
            if (WiFi_pingGateway(PING_PKT_SIZE) == false)
            {
                Display_printf(displayOut, 0, 0,
                               "[wifiThread] ERROR - Cannot ping gateway!",
                               retValue);
                setValue = BLE_WIFI_PROV_FAIL_PING;
                BLE_WiFi_Profile_setParameter(BLE_WIFI_PROV_STATUS_ID, 1,
                                              &setValue);
#ifdef DELETE_PROFILES
                sl_WlanProfileDel(0xFF);
#endif
                break;
            }

            /* Sending the message to the BLE thread to go into "BLE
             * Confirmation" mode and wait for the application to confirm
             * that provisioning happened correctly.
             */
            setValue = BLE_WIFI_PROV_GOOD;
            BLE_WiFi_Profile_setParameter(BLE_WIFI_PROV_STATUS_ID, 1,
                                          &setValue);
            sl_Stop(SL_STOP_TIMEOUT);
            eventPend = BLE_EVT_START_PROVISION;
            mq_send(bleQueueSend, (void*) &eventPend, sizeof(uint32_t), 1);
            curEvent = 0;
            mq_receive(wifiQueueRec, (void*) &curEvent, sizeof(uint32_t),
                       &prio);

            if (curEvent == WIFI_EVT_BLE_PROV_COMPLETE)
            {
                if (WiFi_startConnection() && WiFi_obtainIPAddress())
                {
                    Display_printf(
                            displayOut, 0, 0,
                            "[wifiThread] Device successfully provisioned!");
                    curState = WIFI_PING_LOOP;
                    continue;
                }
                else
                {
                    setValue = BLE_WIFI_PROV_TIMEOUT;
                    BLE_WiFi_Profile_setParameter(BLE_WIFI_PROV_STATUS_ID, 1,
                                                  &setValue);

                    Display_printf(
                            displayOut,
                            0,
                            0,
                            "[wifiThread] Could not reconnect to common network!");
#ifdef DELETE_PROFILES
                    sl_WlanProfileDel(0xFF);
#endif
                }

            }
            else
            {
                Display_printf(displayOut, 0, 0,
                               "[wifiThread] BLE app confirmation failed!");
                /* Attempting to connect to the device and remove the
                 * unconfirmed profile.
                 */

                setValue = BLE_WIFI_PROV_TIMEOUT;
                BLE_WiFi_Profile_setParameter(BLE_WIFI_PROV_STATUS_ID, 1,
                                              &setValue);

                if (WiFi_startConnection() == true)
                {
#ifdef DELETE_PROFILES
                    sl_WlanProfileDel(0xFF);
#endif
                }
            }
        }
            sl_Stop(SL_STOP_TIMEOUT);
            break;
        default:
        {

        }
        }
    }

}

static bool WiFi_startConnection(void)
{
    int32_t retValue;

    /* Try to initially start the SimpleLink driver */
    retValue = sl_Start(NULL, NULL, NULL);

    /* If the connection doesn't come up in station mode we should set the
     * mode again and then restart it  */
    if (retValue > 0 && retValue != ROLE_STA)
    {
        retValue = sl_WlanSetMode(ROLE_STA);

        if (retValue == 0)
        {
            /* Restarting the driver in (hopefully) station mode */
            sl_Stop(SL_STOP_TIMEOUT);
            if (sl_Start(NULL, NULL, NULL) != ROLE_STA)
            {
                return false;
            }
        }
        else
        {
            return false;
        }

    }

    return true;
}

static bool WiFi_obtainIPAddress(void)
{
    uint32_t curEvent = 0;
    uint32_t prio;
    struct timespec ts;

    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += WIFI_IP_TIMEOUT;
    mq_timedreceive(wifiQueueRec, (void*) &curEvent, sizeof(uint32_t), &prio,
                    &ts);

    if (curEvent == WIFI_EVT_SL_CONNECTED)
    {
        return true;
    }
    else
    {
        return false;
    }
}

static bool WiFi_pingGateway(uint32_t bytes)
{
    SlNetAppPingCommand_t pingParams = { 0 };
    SlNetAppPingReport_t pingReport = { 0 };
    int32_t retValue;

    /* Set the ping parameters */
    pingParams.PingIntervalTime = PING_INTERVAL * 1000;
    pingParams.PingSize = bytes;
    pingParams.PingRequestTimeout = PING_TIMEOUT * 1000;
    pingParams.TotalNumberOfAttempts = 1;
    pingParams.Flags = 0;
    pingParams.Ip = Provisioning_ControlBlock.gatewayIP;

    Display_printf(displayOut, 0, 0,
                   "[PingReport] Pinging %s with %d byte(s) of data...",
                   gatewayIPString, bytes);

    retValue = sl_NetAppPing((SlNetAppPingCommand_t*) &pingParams,
    SL_AF_INET,
                             (SlNetAppPingReport_t*) &pingReport,
                             NULL);

    if (retValue < 0)
    {
        Display_printf(displayOut, 0, 0,
                       "[PingReport] Could not ping gateway!! (err: %d)",
                       retValue);
    }
    else if (pingReport.PacketsReceived
            > 0&& pingReport.PacketsReceived <= NO_OF_ATTEMPTS)
    {
        Display_printf(displayOut, 0, 0,
                       "[PingReport] Ping to %s complete. Response: %dms",
                       gatewayIPString, pingReport.AvgRoundTime);
        return true;
    }

    return false;
}

/*
 * This is the key handler for SW3. Since SW2 on the CC3220 LaunchPad is
 * connected to a UART pin that we are using we cannot use SW2. SW3 performs
 * three different functions, depending on how long you hold the button:
 * 1) One fast press (less than three seconds) will start advertisement
 * 2) Holding the button down for three seconds will cause the provisioning
 * mode to switch between BLE mode and AP mode
 * 3) Holding the button down for six seconds will cause the device to go into
 * SBL mode and update the CC26xx network processor.
 */
static void BLE_keyHandler(void)
{
    uint32_t ii;
    uint32_t eventPend;

    GPIO_disableInt(Board_GPIO_BUTTON1);

    /* Delay for switch debounce */
    for (ii = 0; ii < 20000; ii++)
        ;

    GPIO_clearInt(Board_GPIO_BUTTON1);
    GPIO_enableInt(Board_GPIO_BUTTON1);

    /* Initial button press, so start the timer */
    if (timerIsRunning == false)
    {
        Timer_start(buttonTimer);
    }
    /* Button has been held down between 0 - 3 seconds,
     * start advertisement */
    else if (timerIsRunning && !threeSecondButtonPress)
    {
        Timer_stop(buttonTimer);
        Timer_close(buttonTimer);
        buttonTimer = Timer_open(Board_TIMER0, &timerParams);

        Provisioning_ControlBlock.resolvedIP = 0;
    }
    /* Button has been held for at least three seconds,
     * switch provisioning modes */
    else if (timerIsRunning && threeSecondButtonPress)
    {
        Timer_close(buttonTimer);
        buttonTimer = Timer_open(Board_TIMER0, &timerParams);

        threeSecondButtonPress = false;
        switchProvisioningMode = true;

        /* Tell AP thread to switch to BLE mode */
        eventPend = BLE_EVT_SWITCH_PROV;

        /* Tell BLE_IDLE queue that we have switched provisioning modes */
        mq_send(bleQueueSend, (void*) &eventPend, sizeof(uint32_t), 1);
    }

    /* Flip the timer status bit */
    timerIsRunning = !timerIsRunning;
}

//*****************************************************************************
//                 BLE Callbacks
//*****************************************************************************
/*
 * This is a callback operating in the NPI task.
 * These are events this application has registered for.
 */
static void BLE_processSNPEventCB(uint16_t event, snpEventParam_t *param)
{
    uint32_t eventPend;

    switch (event)
    {
    case SNP_CONN_EST_EVT:
    {
        snpConnEstEvt_t * connEstEvt = (snpConnEstEvt_t *) param;

        /* Update Peer Addr String */
        connHandle = connEstEvt->connHandle;
        ProfileUtil_convertBdAddr2Str(&peerstr[peerstrIDX], connEstEvt->pAddr);

        /* Notify state machine of established connection */
        eventPend = BLE_EVT_CONN_EST;
        mq_send(bleQueueSend, (void*) &eventPend, sizeof(uint32_t), 1);
    }
        break;

    case SNP_CONN_TERM_EVT:
    {
        connHandle = BLE_DEFAULT_CONN_HANDLE;

        /* Notify state machine of disconnection event */
        eventPend = BLE_EVT_CONN_TERM;
        mq_send(bleQueueSend, (void*) &eventPend, sizeof(uint32_t), 1);
    }
        break;

    case SNP_ADV_STARTED_EVT:
    {
        snpAdvStatusEvt_t *advEvt = (snpAdvStatusEvt_t *) param;
        if (advEvt->status == SNP_SUCCESS)
        {
            /* Notify state machine of Advertisement Enabled */
            eventPend = BLE_EVT_ADV_ENB;
            mq_send(bleQueueSend, (void*) &eventPend, sizeof(uint32_t), 1);
        }
        else
        {
            eventPend = BLE_EVT_ERROR;
            mq_send(bleQueueSend, (void*) &eventPend, sizeof(uint32_t), 1);
        }
    }
        break;

    case SNP_ADV_ENDED_EVT:
    {
        snpAdvStatusEvt_t * advEvt = (snpAdvStatusEvt_t *) param;
        if (advEvt->status == SNP_SUCCESS)
        {
            /* Notify state machine of Advertisement Disabled */
            eventPend = BLE_EVT_ADV_END;
            mq_send(bleQueueSend, (void*) &eventPend, sizeof(uint32_t), 1);
        }
    }
        break;
    case SNP_SECURITY_EVT:
    {
        snpSecurityEvt_t * secEvt = (snpSecurityEvt_t *) param;

        /* Check if peripheral is already bonded
         * or if authentication was a success */
        if (secEvt->state == SNP_GAPBOND_PAIRING_STATE_BONDED
                || (secEvt->state == SNP_GAPBOND_PAIRING_STATE_COMPLETE
                        && secEvt->status == SNP_SUCCESS))
        {
            eventPend = BLE_EVT_PAIRED;
            mq_send(bleQueueSend, (void*) &eventPend, sizeof(uint32_t), 1);
        }
        /* Wrong passkey was entered */
        else if (secEvt->status == SNP_PAIRING_FAILED_CONFIRM_VALUE)
        {
            eventPend = BLE_EVT_WRONG_PASSKEY;
            mq_send(bleQueueSend, (void*) &eventPend, sizeof(uint32_t), 1);
        }

    }
        break;

    case SNP_AUTHENTICATION_EVT:
    {
        snpAuthenticationEvt_t * authEvt = (snpAuthenticationEvt_t *) param;
        if (authEvt->display == 1)
        {
            eventPend = BLE_EVT_START_PAIR;
            mq_send(bleQueueSend, (void*) &eventPend, sizeof(uint32_t), 1);
        }
    }
        break;

    default:
        break;
    }
}

static void BLE_SPWriteCB(uint8_t charID)
{
    uint8_t startValue;
    uint32_t eventPend;

    switch (PROFILE_ID_CHAR(charID))
    {
    case BLE_WIFI_PROV_STATUS_CHAR:
        switch (PROFILE_ID_CHARTYPE(charID))
        {
        case PROFILE_VALUE:
            break;
        default:
            /* Should not receive other types */
            break;
        }
        break;
    case BLE_WIFI_PROV_SSID_CHAR:
        switch (PROFILE_ID_CHARTYPE(charID))
        {
        case PROFILE_VALUE:
            break;
        default:
            /* Should not receive other types */
            break;
        }
        break;
    case BLE_WIFI_PROV_PASS_CHAR:
        switch (PROFILE_ID_CHARTYPE(charID))
        {
        case PROFILE_VALUE:
            break;
        default:
            /* Should not receive other types */
            break;
        }
        break;
    case BLE_WIFI_PROV_DEVNAME_CHAR:
        switch (PROFILE_ID_CHARTYPE(charID))
        {
        case PROFILE_VALUE:
            break;
        default:
            /* Should not receive other types */
            break;
        }
        break;
    case BLE_WIFI_PROV_START_CHAR:
        switch (PROFILE_ID_CHARTYPE(charID))
        {
        case PROFILE_VALUE:
            BLE_WiFi_Profile_getParameter(BLE_WIFI_PROV_START_ID, &startValue);
            if (startValue == BLE_START_PROVISION)
            {
                eventPend = BLE_EVT_START_PROVISION;
                mq_send(bleQueueSend, (void*) &eventPend, sizeof(uint32_t), 1);
            }
            else if (startValue == BLE_COMPLETE_PROVISION)
            {
                eventPend = BLE_EVT_PROV_COMPLETE;
                mq_send(bleQueueSend, (void*) &eventPend, sizeof(uint32_t), 1);
            }
            break;
        default:
            /* Should not receive other types */
            break;
        }
        break;
    default:
        /* Other Characteristics not writable */
        break;
    }
}

static void BLE_SPcccdCB(uint8_t charID, uint16_t value)
{
    switch (PROFILE_ID_CHAR(charID))
    {
    case BLE_WIFI_PROV_NOTIFY_CHAR:
        switch (PROFILE_ID_CHARTYPE(charID))
        {
        case PROFILE_CCCD:
            break;

        default:
            /* Should not receive other types */
            break;
        }
        break;

    default:
        break;
    }
}

/*******************************************************************************
 * This is a callback operating in the NPI task.
 * These are Asynchronous indications.
 ******************************************************************************/
static void BLE_asyncCB(uint8_t cmd1, void *pParams)
{
    uint32_t eventPend;

    switch (SNP_GET_OPCODE_HDR_CMD1(cmd1))
    {
    case SNP_DEVICE_GRP:
    {
        switch (cmd1)
        {
        case SNP_POWER_UP_IND:
            /* Notify state machine of Power Up Indication */
            eventPend = BLE_EVT_PUI;
            mq_send(bleQueueSend, (void*) &eventPend, sizeof(uint32_t), 1);
            break;

        case SNP_HCI_CMD_RSP:
        {
            snpHciCmdRsp_t *hciRsp = (snpHciCmdRsp_t *) pParams;
            switch (hciRsp->opcode)
            {
            case SNP_HCI_OPCODE_READ_BDADDR:
                /* Update NWP Addr String */
                ProfileUtil_convertBdAddr2Str(&nwpstr[nwpstrIDX],
                                              hciRsp->pData);
                break;
            default:
                break;
            }
        }
            break;

        case SNP_EVENT_IND:
            /* Notify state machine of Advertisement Enabled */
            eventPend = BLE_EVT_ADV_ENB;
            mq_send(bleQueueSend, (void*) &eventPend, sizeof(uint32_t), 1);
            break;

        default:
            break;
        }
    }
        break;

    default:
        break;
    }
}

static void BLE_timerHandler(Timer_Handle handle)
{
    uint32_t eventPend;

    /* If we are in the ping loop we will send the SBL state change from the
     * Wi-Fi thread after we have disconnected from the Wi-Fi NWP.
     */
    if (threeSecondButtonPress == false)
    {
        /* Button has been held for 3 seconds */
        threeSecondButtonPress = true;
        Timer_start(buttonTimer);
    }
    else
    {
        /* Button has been held for 6 seconds - perform firmware update */
        if (Provisioning_ControlBlock.resolvedIP != 0)
        {
            sblUpdateInitiated = true;
            Provisioning_ControlBlock.resolvedIP = 0;
        }
        else
        {
            eventPend = BLE_EVT_BSL_BUTTON;
            mq_send(bleQueueSend, (void*) &eventPend, sizeof(uint32_t), 1);
        }
        threeSecondButtonPress = false;

        /* Flip the timer status bit */
        timerIsRunning = !timerIsRunning;
    }

}
