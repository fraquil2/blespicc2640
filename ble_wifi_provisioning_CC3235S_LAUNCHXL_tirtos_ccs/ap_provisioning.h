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

#ifndef __PROVISIONING_H__
#define __PROVISIONING_H__

/* TI-DRIVERS Header files */
//#include <ti/drivers/net/wifi/simplelink.h>
#include <stdint.h>

/*!
 *  \brief  Provisioning events
 */
typedef enum
{
    PrvnEvent_SwitchProv,
    PrvnEvent_Triggered,
    PrvnEvent_Started,
    PrvnEvent_StartFailed,
    PrvnEvent_ConfirmationSuccess,
    PrvnEvent_ConfirmationFailed,
    PrvnEvent_Stopped,
    PrvnEvent_WaitForConn,
    PrvnEvent_Timeout,
    PrvnEvent_Error,
    PrvnEvent_Max
} PrvnEvent;

/*!
 *  \brief  Provisioning states
 */
typedef enum
{
    PrvnState_SwitchProvMode,
    PrvnState_Init,
    PrvnState_Idle,
    PrvnState_WaitForConfirmation,
    PrvnState_Completed,
    PrvnState_Error,
    PrvnState_Max

} PrvnState;

typedef enum
{
    AppStatusBits_NwpInit = 0,
    AppStatusBits_Connection = 1, /* If this bit is set: the device is connected to the AP or client is connected to device (AP) */
    AppStatusBits_IpLeased = 2, /* If this bit is set: the device has leased IP to any connected client */
    AppStatusBits_IpAcquired = 3, /* If this bit is set: the device has acquired an IP */
    AppStatusBits_SmartconfigStart = 4, /* If this bit is set: the SmartConfiguration process is started from SmartConfig app */
    AppStatusBits_P2pDevFound = 5, /* If this bit is set: the device (P2P mode) found any p2p-device in scan */
    AppStatusBits_P2pReqReceived = 6, /* If this bit is set: the device (P2P mode) found any p2p-negotiation request */
    AppStatusBits_ConnectionFailed = 7, /* If this bit is set: the device(P2P mode) connection to client(or reverse way) is failed */
    AppStatusBits_PingDone = 8, /* If this bit is set: the device has completed the ping operation */
    AppStatusBits_Ipv6lAcquired = 9, /* If this bit is set: the device has acquired an IPv6 address */
    AppStatusBits_Ipv6gAcquired = 10, /* If this bit is set: the device has acquired an IPv6 address */
    AppStatusBits_AuthenticationFailed = 11,
    AppStatusBits_ResetRequired = 12,
} AppStatusBits;

typedef enum
{
    ControlMessageType_Switch2 = 2,
    ControlMessageType_Switch3 = 3,
    ControlMessageType_ResetRequired = 4,
    ControlMessageType_ControlMessagesMax = 0xFF
} ControlMessageType;

/* Status keeping MACROS */

#define SET_STATUS_BIT(status_variable, bit) status_variable |= (1<<(bit))

#define CLR_STATUS_BIT(status_variable, bit) status_variable &= ~(1<<(bit))

#define GET_STATUS_BIT(status_variable, bit)    \
                                (0 != (status_variable & (1<<(bit))))
#define IS_NW_PROCSR_ON(status_variable)    \
                GET_STATUS_BIT(status_variable, AppStatusBits_NwpInit)

#define IS_CONNECTED(status_variable)       \
                GET_STATUS_BIT(status_variable, AppStatusBits_Connection)

#define IS_IP_LEASED(status_variable)       \
                GET_STATUS_BIT(status_variable, AppStatusBits_IpLeased)

#define IS_IP_ACQUIRED(status_variable)     \
                GET_STATUS_BIT(status_variable, AppStatusBits_IpAcquired)

#define IS_IPV6L_ACQUIRED(status_variable)  \
                GET_STATUS_BIT(status_variable, AppStatusBits_Ipv6lAcquired)

#define IS_IPV6G_ACQUIRED(status_variable)  \
                GET_STATUS_BIT(status_variable, AppStatusBits_Ipv6gAcquired)

#define IS_SMART_CFG_START(status_variable) \
                GET_STATUS_BIT(status_variable, AppStatusBits_SmartconfigStart)

#define IS_P2P_DEV_FOUND(status_variable)   \
                GET_STATUS_BIT(status_variable, AppStatusBits_P2pDevFound)

#define IS_P2P_REQ_RCVD(status_variable)    \
                GET_STATUS_BIT(status_variable, AppStatusBits_P2pReqReceived)

#define IS_CONNECT_FAILED(status_variable)  \
                GET_STATUS_BIT(status_variable, AppStatusBits_Connection)

#define IS_PING_DONE(status_variable)       \
                GET_STATUS_BIT(status_variable, AppStatusBits_PingDone)

#define IS_AUTHENTICATION_FAILED(status_variable)   \
            GET_STATUS_BIT(status_variable, AppStatusBits_AuthenticationFailed)

/****************************************************************************
 GLOBAL VARIABLES
 ****************************************************************************/


//****************************************************************************
//                      FUNCTION PROTOTYPES
//****************************************************************************

//*****************************************************************************
//
//! \brief This function signals the application events
//!
//! \param[in]  None
//!
//! \return None
//!
//****************************************************************************
void AP_signalProvisioningEvent(PrvnEvent event);

//*****************************************************************************
//
//! \brief This function gets the current provisioning state
//!
//! \param[in]  None
//!
//! \return provisioning state
//!
//****************************************************************************
PrvnState AP_getProvisioningState();

//*****************************************************************************
//
//! \brief This function stops provisioning process
//!
//! \param[in]  None
//!
//! \return SL_RET_CODE_PROVISIONING_IN_PROGRESS if provisioning was running,
//!         otherwise 0
//!
//****************************************************************************
int32_t AP_provisioningStop(void);

//*****************************************************************************
//
//! \brief This is the main provisioning task
//!
//! \param[in]  None
//!
//! \return None
//!
//****************************************************************************

void * provisioningTask(void *pvParameters);

#endif
