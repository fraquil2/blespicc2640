/*
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
 */
#ifndef __BLE_PROVISIONING_H
#define __BLE_PROVISIONING_H

#ifdef __cplusplus
extern "C"
{
#endif

/* Application Version and Naming*/
#define APPLICATION_NAME         "BLE PROVISIONING"
#define APPLICATION_VERSION      "1.00.00.00"

/* USER's defines */
#define SPAWN_TASK_PRIORITY                     (9)
#define TASK_STACK_SIZE                         (2048)
#define TIMER_TASK_STACK_SIZE                   (1024)
#define DISPLAY_TASK_STACK_SIZE                 (512)
#define SL_STOP_TIMEOUT                         (200)
#define RECONNECTION_ESTABLISHED_TIMEOUT_SEC    (15)       /* 15 seconds */
#define CONNECTION_PHASE_TIMEOUT_SEC            (2)        /* 2 seconds */
#define PING_TIMEOUT_SEC                        (1)        /* 1 second */

/* Enable UART Log */
#define LOG_MESSAGE_ENABLE
#define LOG_MESSAGE UART_PRINT

#define HTTPS_ENABLE        (1)
#define SC_KEY_ENABLE       (0)
#define SECURED_AP_ENABLE   (0)

#define AP_SEC_TYPE         (SL_WLAN_SEC_TYPE_WPA_WPA2)
#define AP_SEC_PASSWORD     "1234567890"
#define SC_KEY              "1234567890123456"

#define SSID_LEN_MAX            (32)
#define BSSID_LEN_MAX           (6)

/* For Debug - Start Provisioning for each reset, or stay connected */
/* Used mainly in case of provisioning robustness tests             */
#define FORCE_PROVISIONING   (0)

#define ASSERT_ON_ERROR(error_code) \
            {\
            /* Handling the error-codes is specific to the application */ \
            if (error_code < 0) {Display_print1(displayOut, 0, 0, \
                                                "ERROR! Code: %d ", \
                                                error_code); \
            } \
            /* else, continue w/ execution */ \
            }\

#define SSL_SERVER_KEY          "dummy-root-ca-cert-key"
#define SSL_SERVER_CERT         "dummy-root-ca-cert"

/* Application's states */
/* BLE States */
typedef enum
{
    BLE_RESET,
    BLE_IDLE,
    BLE_START_ADV,
    BLE_CONNECTED,
    BLE_PAIRED,
    BLE_CANCEL_ADV,
    BLE_SBL,
} ble_states_t;

/* WIFI States */
typedef enum
{
    WIFI_INITIALIZE,
    WIFI_IDLE,
    WIFI_PING_LOOP,
    WIFI_ATTEMPT_PROVISION,
    WIFI_ERROR,
} wifi_states_t;

typedef struct Provisioning_ControlBlock_t
{
    uint32_t status; /* SimpleLink Status */
    uint32_t gatewayIP; /* Network Gateway IP address */
    uint32_t resolvedIP; /* Resolved IP address */
    uint8_t connectionSSID[SSID_LEN_MAX + 1]; /* Connection SSID */
    uint8_t ssidLen; /* Connection SSID */
    uint8_t connectionBSSID[BSSID_LEN_MAX]; /* Connection BSSID */
    bool isBleProvisioningActive; /* Determine provisioning mode */
} Provisioning_CB;

/* Application specific status/error codes */
typedef enum
{
    /* Choosing -0x7D0 to avoid overlap w/ host-driver's error codes */
    AppStatusCodes_LanConnectionFailed = -0x7D0,
    AppStatusCodes_InternetConnectionFailed = AppStatusCodes_LanConnectionFailed
            - 1,
    AppStatusCodes_DeviceNotInStationMode = AppStatusCodes_InternetConnectionFailed
            - 1,
    AppStatusCodes_StatusCodeMax = -0xBB8
} AppStatusCodes;

/* Advertising interval when device is discoverable
 * (units of 625us, 160=100ms)
 */
#define DEFAULT_ADVERTISING_INTERVAL        160

/* Limited discoverable mode advertises for 30.72s, and then stops
 General discoverable mode advertises indefinitely */
#define DEFAULT_DISCOVERABLE_MODE           SAP_GAP_ADTYPE_FLAGS_GENERAL

/* Minimum connection interval (units of 1.25ms, 80=100ms) if automatic
 parameter update request is enabled */
#define DEFAULT_DESIRED_MIN_CONN_INTERVAL   80

/* Maximum connection interval (units of 1.25ms, 800=1000ms) if automatic
 parameter update request is enabled */
#define DEFAULT_DESIRED_MAX_CONN_INTERVAL   800

/* Company Identifier: Texas Instruments Inc. (13) */
#define TI_COMPANY_ID                        0x000D
#define TI_ST_DEVICE_ID                      0x03
#define TI_ST_KEY_DATA_ID                    0x00

#define nwpstrIDX       8
#define peerstrIDX       8

/* How often to perform periodic event (in usec) */
#define BLE_PERIODIC_EVT_PERIOD               5000000
#define BLE_DEFAULT_CONN_HANDLE               0xFFFF

/* Application Events */
#define BLE_NONE                 0x00000001     // No Event
#define BLE_EVT_PUI              0x00000002     // Power-Up Indication
#define BLE_EVT_ADV_ENB          0x00000004     // Advertisement Enable
#define BLE_EVT_ADV_END          0x00000008     // Advertisement Ended
#define BLE_EVT_CONN_EST         0x00000010     // Connection Established Event
#define BLE_EVT_CONN_TERM        0x00000020     // Connection Terminated Event
#define BLE_EVT_BUTTON_RESET     0x00000040     // RESET
#define BLE_EVT_START_ADV        0x00000080     // Start Advertising
#define BLE_EVT_START_PROVISION  0x00000200     // Start WiFi Provisioning
#define BLE_EVT_PROV_COMPLETE    0x00000400     // Provisioning Complete
#define BLE_EVT_START_PAIR       0x00000600
#define BLE_EVT_PAIRED           0x00000700
#define BLE_EVT_WRONG_PASSKEY    0x00000750
#define BLE_EVT_BSL_BUTTON       0x00000800     // Go into BSL mode
#define BLE_EVT_SWITCH_PROV      0x00000850
#define BLE_EVT_ERROR            0x80000000     // Error

#define WIFI_NONE                    0x00000001     // No Event
#define WIFI_EVT_BLE_INITI           0x00000002     // Wait for SNP to start
#define WIFI_EVT_RESET               0x00000003
#define WIFI_EVT_BLE_PROV_START      0x00000004     // Wait for provisioning to start
#define WIFI_EVT_SL_STARTED          0x00000008     // SL Initialized
#define WIFI_EVT_SL_CONNECTED        0x00000020     // SL Connected
#define WIFI_EVT_BLE_PROV_COMPLETE   0x00000040     // Provision Completed
#define WIFI_EVT_REPROVISION         0x00000080     // Reprovision Device
#define WIFI_EVT_BLE_TIME            0x00000200     // BLE Timeout
#define WIFI_EVT_CONF_FAIL           0x00000400     // Provisioning confirmation failed
#define WIFI_EVT_ERROR               0x80000000     // Error

#define BLE_WIFI_PROV_GOOD         0x00
#define BLE_WIFI_PROV_FAIL_IP      0xFC
#define BLE_WIFI_PROV_FAIL_CON     0xFD
#define BLE_WIFI_PROV_FAIL_PING    0xFE
#define BLE_WIFI_PROV_TIMEOUT      0xFF

/* Ping Characteristics */
#define PING_INTERVAL                           (2)    /* 2 seconds */
#define PING_TIMEOUT                            (2)    /* 2 seconds */
#define PING_PKT_SIZE                           (20)   /* In bytes */
#define NO_OF_ATTEMPTS                          (4)

/* Timeout for connecting or acquiring an IP (in seconds) */
#define WIFI_CONNECT_TIMEOUT 30
#define WIFI_IP_TIMEOUT 5

/* Application Defines */
#define BLE_START_PROVISION     0x01
#define BLE_COMPLETE_PROVISION  0x02

/* Allow deletion of all stored profiles when:
 * 1) Provisioning is started
 * 2) Provisioning fails */
#define DELETE_PROFILES                         (1)

#ifdef __cplusplus
}
#endif

#endif /* __BOARD_H */
