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

//*****************************************************************************
//
//! \addtogroup out_of_box
//! @{
//
//*****************************************************************************
/* standard includes */
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <mqueue.h>

/* driverlib Header files */
#include <ti/devices/cc32xx/inc/hw_types.h>
#include <ti/devices/cc32xx/inc/hw_memmap.h>
#include <ti/devices/cc32xx/driverlib/rom.h>
#include <ti/devices/cc32xx/driverlib/rom_map.h>
#include <ti/devices/cc32xx/driverlib/prcm.h>
#include <ti/devices/cc32xx/driverlib/timer.h>

/* TI-DRIVERS Header files */
#include <Board.h>
#include <ti/drivers/GPIO.h>
#include <ti/display/Display.h>
#include <ti/drivers/net/wifi/simplelink.h>
#include <ti/drivers/net/wifi/wlan.h>
#include <ti/drivers/Timer.h>

/* POSIX Header files */
#include <pthread.h>
#include <time.h>
#include <semaphore.h>

/* Local Files */
#include "ble_provisioning.h"
#include "ap_provisioning.h"

/*In sec. Used for connecting to stored profile */
#define PROFILE_ASYNC_EVT_TIMEOUT			(5)
/* Provisioning inactivity timeout in seconds */
#define AP_PROVISIONING_INACTIVITY_TIMEOUT		(600)

#define ROLE_SELECTION_BY_SL     			(0xFF)

#define SL_STOP_TIMEOUT         (200)
#define OCP_REGISTER_INDEX      (0)
#define OCP_REGISTER_OFFSET     (10)

sem_t AP_connectionAsyncSem;
Provisioning_CB Provisioning_ControlBlock;
static Timer_Handle apTimer;

/*!
 *  \brief  Provisioning modes
 */
typedef enum
{
    PrvsnMode_AP, /* AP provisioning (AP role) */
    PrvsnMode_SC, /* Smart Config provisioning (STA role) */
    PrvsnMode_APSC /* AP + Smart Config provisioning (AP role) */

} PrvsnMode;

/** By default, setting the provisioning mode to AP + Smart Config.
 * Other values could be PrvsnMode_SC or PrvsnMode_AP
 */
#define PROVISIONING_MODE   PrvsnMode_APSC

/*!
 *  \brief  Provisioning status
 */
typedef enum
{
    PrvsnStatus_Stopped, PrvsnStatus_InProgress

} PrvsnStatus;

/*
 *  \brief  Application state's context
 */
typedef struct _Provisioning_AppContext_t_
{
    PrvnState currentState; /* Current state of provisioning */
    uint32_t pendingEvents; /* Events pending to be processed */

    uint8_t role; /* SimpleLink's role - STATION/AP/P2P */

    uint8_t defaultRole; /* SimpleLink's default role, try not to change this */
    PrvsnMode provisioningMode; /* Provisioning Mode */
    PrvsnStatus provisioningStatus; /* */
} Provisioning_AppContext;

/*!
 *  \brief  Function pointer to the event handler
 */
typedef int32_t (*fptr_EventHandler)(void);

/*!
 *  \brief  Entry in the lookup table
 */
typedef struct
{
    fptr_EventHandler p_evtHndl; /* Pointer to the event handler */
    PrvnState nextState; /* Next state of provisioning */

} s_TblEntry;

/****************************************************************************
 LOCAL FUNCTION PROTOTYPES
 ****************************************************************************/

//*****************************************************************************
//
//! \brief This function initializes provisioning process
//!
//! \param[in]  None
//!
//! \return None
//!
//****************************************************************************
static void AP_provisioningInit(void);

//*****************************************************************************
//
//! \brief This function starts provisioning process
//!
//! \param[in]  None
//!
//! \return 0 on success, negative value otherwise
//!
//****************************************************************************
static int32_t AP_provisioningStart(void);

//*****************************************************************************
//
//! \brief This function switches the provisioning mode between
//! BLE provisioning and AP/SC provisioning
//!
//! \param[in]  None
//!
//! \return 0 on success, negative value otherwise
//!
//****************************************************************************
static int32_t AP_switchProvisioningMode(void);

//*****************************************************************************
//
//! \brief main provisioning loop
//!
//! \param[in]  None
//!
//! \return 0 on success, negative value otherwise
//!
//****************************************************************************
static int32_t AP_provisioningAppTask(void);

//*****************************************************************************
//! \brief This function puts the device in its default state. It:
//!           - Set the mode to AP
//!           - Configures connection policy to Auto
//!           - Deletes all the stored profiles
//!           - Enables DHCP
//!		  - Disable IPV6
//!           - Disables Scan policy
//!           - Sets Tx power to maximum
//!           - Sets power policy to normal
//!           - Unregister mDNS services
//!           - Remove all filters
//!
//!           IMPORTANT NOTE - This is an example reset function, user must
//!           update this function to match the application settings.
//!
//! \param   none
//! \return  On success, zero is returned. On error, negative is returned
//*****************************************************************************
static int32_t AP_configureSimpleLinkToDefaultState(void);

//*****************************************************************************
//
//! \brief This function starts the SimpleLink in the configured role. 
//!		 The device notifies the host asynchronously when the initialization
//!      is completed
//!
//! \param[in]  role	Device shall be configured in this role
//!
//! \return 0 on success, negative value otherwise
//!
//****************************************************************************
static int32_t AP_initSimplelink(uint8_t const role);

//*****************************************************************************
//
//! \brief This function handles 'APP_EVENT_STARTED' event
//!
//! \param[in]  None
//!
//! \return 0 on success, negative value otherwise
//!
//****************************************************************************
static int32_t AP_handleStartedEvent(void);



//*****************************************************************************
//
//! \brief internal error detection during provisioning process
//!
//! \param[in]  None
//!
//!
//****************************************************************************
static int32_t AP_reportError(void);

//*****************************************************************************
//
//! \brief internal report current state during provisioning process
//!
//! \param[in]  None
//!
//!
//****************************************************************************
static int32_t AP_reportSM(void);

//*****************************************************************************
//
//! \brief steps following a successful provisioning process
//!
//! \param[in]  None
//!
//! \return 0 on success, negative value otherwise
//!
//****************************************************************************
static int32_t AP_reportSuccess(void);

//*****************************************************************************
//
//! \brief wait for connection following a successful provisioning process
//!
//! \param[in]  None
//!
//! \return 0 on success, negative value otherwise
//!
//****************************************************************************
static int32_t AP_waitForConnection(void);

//*****************************************************************************
//
//! Notify if device return to factory image
//!
//! \param  None
//!
//! \return None
//
//*****************************************************************************
static void AP_notifyReturnToFactoryImage(void);

/****************************************************************************
 GLOBAL VARIABLES
 ****************************************************************************/
/*!
 *  \brief  Application state's context
 */
static Provisioning_AppContext apStateContext;

/* Used for log messages */
extern Display_Handle displayOut;

/*!
 *  \brief   Application lookup/transition table
 */
const s_TblEntry apTransitionTable[PrvnState_Max][PrvnEvent_Max] = {
        /* PrvnState_SwitchProvMode */
        {
         /* Event: PrvnEvent_SwitchProv */
         { AP_switchProvisioningMode, PrvnState_SwitchProvMode },
        /* Event: PrvnEvent_Triggered */
        { AP_provisioningStart, PrvnState_Idle },
          /* Event: PrvnEvent_Started */
          { AP_reportError, PrvnState_Error },
          /* Event: PrvnEvent_StartFailed */
          { AP_reportError, PrvnState_Error },
          /* Event: PrvnEvent_ConfirmationSuccess */
          { AP_reportError, PrvnState_Error },
          /* Event: PrvnEvent_ConfirmationFailed */
          { AP_reportError, PrvnState_Error },
          /* Event: PrvnEvent_Stopped */
          { AP_reportSM, PrvnState_Init },
          /* Event: PrvnEvent_WaitForConn */
          { AP_reportSM, PrvnState_Error },
          /* Event: PrvnEvent_Timeout */
          { AP_reportError, PrvnState_Error },
          /* Event: PrvnEvent_Error */
          { AP_reportError, PrvnState_Error },},
        /* PrvnState_Init */
        {
         /* Event: PrvnEvent_SwitchProv */
         { AP_switchProvisioningMode, PrvnState_SwitchProvMode },
        /* Event: PrvnEvent_Triggered */
        { AP_provisioningStart, PrvnState_Idle },
          /* Event: PrvnEvent_Started */
          { AP_reportError, PrvnState_Error },
          /* Event: PrvnEvent_StartFailed */
          { AP_reportError, PrvnState_Error },
          /* Event: PrvnEvent_ConfirmationSuccess */
          /* Special case where confirmation is received after application is
           * restarted and NWP is still in provisioning. In this case, need to
           * move to COMPLETED state */
          { AP_reportSM, PrvnState_Completed },
          /* Event: PrvnEvent_ConfirmationFailed */
          { AP_reportError, PrvnState_Error },
          /* Event: PrvnEvent_Stopped */
          { AP_reportSM, PrvnState_Init }, /* in case of auto provisioning */
          /* Event: PrvnEvent_WaitForConn */
          { AP_reportSM, PrvnState_Error },
          /* Event: PrvnEvent_Timeout */
          { AP_reportError, PrvnState_Error },
          /* Event: PrvnEvent_Error */
          { AP_reportError, PrvnState_Error },},
        /* PrvnState_Idle */
        {
         /* Event: PrvnEvent_SwitchProv */
         { AP_switchProvisioningMode, PrvnState_Init },
        /* Event: PrvnEvent_Triggered */
        { AP_reportError, PrvnState_Error },
          /* Event: PrvnEvent_Started */
          { AP_handleStartedEvent, PrvnState_WaitForConfirmation },
          /* Event: PrvnEvent_StartFailed */
          { AP_reportError, PrvnState_Error },
          /* Event: PrvnEvent_ConfirmationSuccess */
          { AP_reportError, PrvnState_Error },
          /* Event: PrvnEvent_ConfirmationFailed */
          { AP_reportError, PrvnState_Error },
          /* Event: PrvnEvent_Stopped */
          { AP_reportSM, PrvnState_Init },
          /* Event: PrvnEvent_WaitForConn */
          { AP_reportSM, PrvnState_Error },
          /* Event: PrvnEvent_Timeout */
          { AP_reportError, PrvnState_Error },
          /* Event: PrvnEvent_Error */
          { AP_reportError, PrvnState_Error },},
        /* PrvnState_WaitForConfirmation */
        {
         /* Event: PrvnEvent_SwitchProv */
        { AP_switchProvisioningMode, PrvnState_Init },
        /* Event: PrvnEvent_Triggered */
        { AP_reportError, PrvnState_Error },
          /* Event: PrvnEvent_Started */
          { AP_reportError, PrvnState_Error },
          /* Event: PrvnEvent_StartFailed */
          { AP_reportSM, PrvnState_Idle },
          /* Event: PrvnEvent_ConfirmationSuccess */
          { AP_reportSM, PrvnState_Completed },
          /* Event: PrvnEvent_ConfirmationFailed */
          { AP_reportSM, PrvnState_WaitForConfirmation },
          /* Event: PrvnEvent_Stopped */
          { AP_reportSM, PrvnState_Init },
          /* Event: PrvnEvent_WaitForConn */
          { AP_reportSM, PrvnState_Error },
          /* Event: PrvnEvent_Timeout */
          { AP_reportError, PrvnState_Error },
          /* Event: PrvnEvent_Error */
          { AP_reportError, PrvnState_Error },},
        /* PrvnState_Completed */
        {
         /* Event: PrvnEvent_SwitchProv */
          { AP_reportError, PrvnState_Error },
        /* Event: PrvnEvent_Triggered */
        { AP_reportError, PrvnState_Error },
          /* Event: PrvnEvent_Started */
          { AP_reportError, PrvnState_Error },
          /* Event: PrvnEvent_StartFailed */
          { AP_reportError, PrvnState_Error },
          /* Event: PrvnEvent_ConfirmationSuccess */
          { AP_reportError, PrvnState_Error },
          /* Event: PrvnEvent_ConfirmationFailed */
          { AP_reportError, PrvnState_Error },
          /* Event: PrvnEvent_Stopped */
          { AP_reportSuccess, PrvnState_Init },
          /* Event: PrvnEvent_WaitForConn */
          /* This state should cover cases where feedback failed but profile
           * exists */
          { AP_waitForConnection, PrvnState_Init },
          /* Event: PrvnEvent_Timeout */
          { AP_reportError, PrvnState_Error },
          /* Event: PrvnEvent_Error */
          { AP_reportError, PrvnState_Error },},
        /* PrvnState_Error */
        {
         /* Event: PrvnEvent_SwitchProv */
       { AP_reportError, PrvnState_Error },
        /* Event: PrvnEvent_Triggered */
        { AP_reportError, PrvnState_Error },
          /* Event: PrvnEvent_Started */
          { AP_reportError, PrvnState_Error },
          /* Event: PrvnEvent_StartFailed */
          { AP_reportError, PrvnState_Error },
          /* Event: PrvnEvent_ConfirmationSuccess */
          { AP_reportError, PrvnState_Error },
          /* Event: PrvnEvent_ConfirmationFailed */
          { AP_reportError, PrvnState_Error },
          /* Event: PrvnEvent_Stopped */
          { AP_reportError, PrvnState_Error },
          /* Event: PrvnEvent_WaitForConn */
          { AP_reportSM, PrvnState_Error },
          /* Event: PrvnEvent_Timeout */
          { AP_reportError, PrvnState_Error },
          /* Event: PrvnEvent_Error */
          { AP_reportError, PrvnState_Error },} };

/*****************************************************************************
 Callback Functions
 *****************************************************************************/

//*****************************************************************************
//
//! \brief The device init callback
//!
//! \param[in]  status	Mode the device is configured in
//!
//! \return None
//!
//****************************************************************************
static void SimpleLinkInitCallback(uint32_t status,
                                   SlDeviceInitInfo_t *DeviceInitInfo)
{
    Provisioning_AppContext * const stateContext = &apStateContext;

    Display_printf(
            displayOut, 0, 0, "[apThread] Device started in %s role",
            (0 == status) ? "Station" :\
 ((2 == status) ? "AP" : "P2P"));

    /* While provisioning is ongoing, the appropriate role is chosen by the
     * device itself, and host can remain agnostic to these details
     */
    if (stateContext->role == ROLE_SELECTION_BY_SL)
    {
        AP_signalProvisioningEvent(PrvnEvent_Started);
    }
    else
    {
        /* Either trigger an error/started event here */
        if (stateContext->role == status)
        {
            AP_signalProvisioningEvent(PrvnEvent_Started);
        }
        else
        {
            Display_printf(
                    displayOut,
                    0,
                    0,
                    "[apThread] But the intended role is %s",
                    (0 == stateContext->role) ?
                            "Station" :\

                            ((2 == stateContext->role) ? "AP" : "P2P"));
            AP_signalProvisioningEvent(PrvnEvent_Error);
        }
    }
}

//*****************************************************************************
//
//! \brief The interrupt handler for the AP provisioning mode timer
//!
//! \param[in]  None
//!
//! \return None
//!
//****************************************************************************
static void AP_timerHandler(Timer_Handle handle)
{
    Timer_stop(apTimer);
    AP_signalProvisioningEvent(PrvnEvent_Timeout);
}

//*****************************************************************************
//                 Local Functions
//*****************************************************************************

//*****************************************************************************
//
//! \brief This function initializes provisioning process
//!
//! \param[in]  None
//!
//! \return None
//!
//****************************************************************************
static void AP_provisioningInit(void)
{
    Provisioning_AppContext * const stateContext = &apStateContext;

    Timer_Params timerParams;
    Timer_Params_init(&timerParams);
    timerParams.period = 5000000;
    timerParams.periodUnits = Timer_PERIOD_US;
    timerParams.timerMode = Timer_ONESHOT_CALLBACK;
    timerParams.timerCallback = AP_timerHandler;

    apTimer = Timer_open(Board_TIMER1, &timerParams);
    if (apTimer == NULL)
    {
        Display_printf(displayOut, 0, 0, "[apThread] Failed to initialize Timer!");
    }

    /* By default, setting the provisioning mode to AP + Smart Config.
     * Other values could be PrvsnMode_SC or PrvsnMode_AP
     */
    stateContext->provisioningMode = PROVISIONING_MODE;
    switch (stateContext->provisioningMode)
    {
    case PrvsnMode_APSC:
        stateContext->defaultRole = ROLE_AP;
        break;

    case PrvsnMode_AP:
        stateContext->defaultRole = ROLE_AP;
        break;

    case PrvsnMode_SC:
        stateContext->defaultRole = ROLE_STA;
        break;
    }

    /* Provisioning has not started yet */
    stateContext->provisioningStatus = PrvsnStatus_Stopped;
    stateContext->currentState = PrvnState_Init;
}

//*****************************************************************************
//
//! \brief This function starts provisioning process
//!
//! \param[in]  None
//!
//! \return 0 on success, negative value otherwise
//!
//****************************************************************************
static int32_t AP_provisioningStart(void)
{
    int32_t retVal = 0;
    Provisioning_AppContext * const stateContext = &apStateContext;
    SlDeviceVersion_t ver = { 0 };
    uint8_t configOpt = 0;
    uint16_t configLen = 0;

    /* Check if provisioning is running */
    /* If auto provisioning - the command stops it automatically */
    /* In case host triggered provisioning - need to stop it explicitly */
    configOpt = SL_DEVICE_GENERAL_VERSION;
    configLen = sizeof(ver);
    retVal = sl_DeviceGet(SL_DEVICE_GENERAL, &configOpt, &configLen,
                          (uint8_t *) (&ver));
    if (SL_RET_CODE_PROVISIONING_IN_PROGRESS == retVal)
    {
        Display_printf(
                displayOut,
                0,
                0,
                "[apThread] Provisioning is already running, stopping it...");
        retVal = sl_WlanProvisioning(SL_WLAN_PROVISIONING_CMD_STOP, ROLE_STA, 0,
        NULL,
                                     0);

        /* Return  SL_RET_CODE_PROVISIONING_IN_PROGRESS to indicate the
         * SM to stay in the same state*/
        return SL_RET_CODE_PROVISIONING_IN_PROGRESS;
    }

    Display_printf(displayOut, 0, 0,
                               "[apThread] AP/SC Provisioning starting!");

    /* IMPORTANT NOTE - This is an example reset function, user must update
     * this function to match the application settings.
     */
    retVal = AP_configureSimpleLinkToDefaultState();

    if (retVal < 0)
    {
        Display_printf(
                displayOut,
                0,
                0,
                "[apThread] Failed to configure the device in its default state");
        return retVal;
    }

    Display_printf(displayOut, 0, 0,
                   "[apThread] Device is configured in default state");

    /* Provisioning has not started yet */
    stateContext->provisioningStatus = PrvsnStatus_Stopped;

    retVal = AP_initSimplelink(stateContext->defaultRole);
    if (retVal < 0)
    {
        Display_printf(displayOut, 0, 0,
                       "[apThread] Failed to initialize the device");
        return retVal;
    }

    return retVal;
}

//*****************************************************************************
//
//! \brief main provisioning loop
//!
//! \param[in]  None
//!
//! \return 0 on success, negative value otherwise
//!
//****************************************************************************
static int32_t AP_provisioningAppTask(void)
{
    Provisioning_AppContext * const stateContext = &apStateContext;
    s_TblEntry *tableEntry = NULL;
    uint16_t eventIdx = 0;
    int32_t retVal = 0;

    for (eventIdx = 0; eventIdx < PrvnEvent_Max; eventIdx++)
    {
        if (0 != (stateContext->pendingEvents & (1 << eventIdx)))
        {
            if (eventIdx != PrvnEvent_Triggered)
            {
                /** Events received - Stop the respective timer if its still
                 * running
                 */
                Timer_stop(apTimer);
            }

            tableEntry =
                    (s_TblEntry *) &apTransitionTable[stateContext->currentState][eventIdx];
            if (NULL != tableEntry->p_evtHndl)
            {
                retVal = tableEntry->p_evtHndl();
                if (retVal == SL_RET_CODE_PROVISIONING_IN_PROGRESS)
                {
                    /* No state transition is required */
                    stateContext->pendingEvents &= ~(1 << eventIdx);
                    continue;
                }
                else if (retVal < 0)
                {
                    Display_printf(
                            displayOut, 0, 0,
                            "[apThread] Event handler failed, error=%d",
                            retVal);

                    /* Let other tasks recover by mcu reset,
                     * e.g. in case of switching to AP mode */
                    while (1)
                    {
                        usleep(1000);
                    }
                }
            }

            if (tableEntry->nextState != stateContext->currentState)
            {
                stateContext->currentState = tableEntry->nextState;
            }

            stateContext->pendingEvents &= ~(1 << eventIdx);
        }

        /* No more events to handle. Break.! */
        if (0 == stateContext->pendingEvents)
        {
            break;
        }
    }

    usleep(1000);

    return retVal;
}

//*****************************************************************************
//! \brief This function puts the device in its default state. It:
//!           - Set the mode to AP
//!           - Configures connection policy to Auto
//!           - Deletes all the stored profiles
//!           - Enables DHCP
//!		  - Disable IPV6
//!           - Disables Scan policy
//!           - Sets Tx power to maximum
//!           - Sets power policy to normal
//!           - Unregister mDNS services
//!           - Remove all filters
//!
//!           IMPORTANT NOTE - This is an example reset function, user must
//!           update this function to match the application settings.
//!
//! \param   none
//! \return  On success, zero is returned. On error, negative is returned
//*****************************************************************************
static int32_t AP_configureSimpleLinkToDefaultState(void)
{
    SlWlanRxFilterOperationCommandBuff_t RxFilterIdMask;

    uint8_t ucConfigOpt = 0;
    uint16_t ifBitmap = 0;
    uint8_t ucPower = 0;

    int32_t ret = -1;
    int32_t mode = -1;

    memset(&RxFilterIdMask, 0, sizeof(SlWlanRxFilterOperationCommandBuff_t));

    /* Start Simplelink - Blocking mode */
    mode = sl_Start(0, 0, 0);
    if (SL_RET_CODE_DEV_ALREADY_STARTED != mode)
    {
        ASSERT_ON_ERROR(mode);
    }

    /* If the device is not in AP mode, try configuring it in AP mode
     * in case device is already started (got SL_RET_CODE_DEV_ALREADY_STARTED
     * error code), then mode would remain -1 and in this case we do not know
     * the role. Move to AP role anyway	*/
    if (ROLE_AP != mode)
    {

        /* Switch to AP role and restart */
        ret = sl_WlanSetMode(ROLE_AP);
        ASSERT_ON_ERROR(ret);

        ret = sl_Stop(SL_STOP_TIMEOUT);
        ASSERT_ON_ERROR(ret);

        ret = sl_Start(0, 0, 0);
        ASSERT_ON_ERROR(ret);

        /* Check if the device is in AP again */
        if (ROLE_AP != ret)
        {
            return ret;
        }
    }

    /* Set connection policy to Auto (no AutoProvisioning)  */
    ret = sl_WlanPolicySet(SL_WLAN_POLICY_CONNECTION,
                           SL_WLAN_CONNECTION_POLICY(1, 0, 0, 0), NULL, 0);
    ASSERT_ON_ERROR(ret);

#ifdef DELETE_PROFILES
    /* Remove all profiles */
    ret = sl_WlanProfileDel(0xFF);
    ASSERT_ON_ERROR(ret);
#endif


    /* Enable DHCP client */
    ret = sl_NetCfgSet(SL_NETCFG_IPV4_STA_ADDR_MODE, SL_NETCFG_ADDR_DHCP, 0, 0);
    ASSERT_ON_ERROR(ret);

    /* Disable IPV6 */
    ifBitmap = 0;
    ret = sl_NetCfgSet(SL_NETCFG_IF, SL_NETCFG_IF_STATE, sizeof(ifBitmap),
                       (uint8_t *) &ifBitmap);
    ASSERT_ON_ERROR(ret);

    /* Disable scan */
    ucConfigOpt = SL_WLAN_SCAN_POLICY(0, 0);
    ret = sl_WlanPolicySet(SL_WLAN_POLICY_SCAN, ucConfigOpt, NULL, 0);
    ASSERT_ON_ERROR(ret);

    /* Set Tx power level for station mode
     * Number between 0-15, as dB offset from max power - 0 will set max power */
    ucPower = 0;
    ret = sl_WlanSet(SL_WLAN_CFG_GENERAL_PARAM_ID,
    SL_WLAN_GENERAL_PARAM_OPT_STA_TX_POWER,
                     1, (uint8_t *) &ucPower);
    ASSERT_ON_ERROR(ret);

    /* Set PM policy to normal */
    ret = sl_WlanPolicySet(SL_WLAN_POLICY_PM, SL_WLAN_NORMAL_POLICY, NULL, 0);
    ASSERT_ON_ERROR(ret);

    /* Unregister mDNS services */
    ret = sl_NetAppMDNSUnRegisterService(0, 0, 0);
    ASSERT_ON_ERROR(ret);

    /* Remove  all 64 filters (8*8) */
    memset(RxFilterIdMask.FilterBitmap, 0xFF, 8);
    ret = sl_WlanSet(SL_WLAN_RX_FILTERS_ID,
    SL_WLAN_RX_FILTER_REMOVE,
                     sizeof(SlWlanRxFilterOperationCommandBuff_t),
                     (uint8_t *) &RxFilterIdMask);
    ASSERT_ON_ERROR(ret);

    ret = sl_Stop(SL_STOP_TIMEOUT);
    ASSERT_ON_ERROR(ret);

    return ret;
}

//*****************************************************************************
//
//! \brief This function starts the SimpleLink in the configured role. 
//!		 The device notifies the host asynchronously when the initialization
//!      is completed
//!
//! \param[in]  role	Device shall be configured in this role
//!
//! \return 0 on success, negative value otherwise
//!
//****************************************************************************
static int32_t AP_initSimplelink(uint8_t const role)
{
    Provisioning_AppContext * const stateContext = &apStateContext;
    int32_t retVal = -1;

    stateContext->role = role;
    stateContext->pendingEvents = 0;

    retVal = sl_Start(0, 0, (P_INIT_CALLBACK) SimpleLinkInitCallback);
    ASSERT_ON_ERROR(retVal);

    /* Start timer */
    Timer_start(apTimer);

    return retVal;
}

//*****************************************************************************
//
//! \brief This function handles 'APP_EVENT_STARTED' event
//!
//! \param[in]  None
//!
//! \return 0 on success, negative value otherwise
//!
//****************************************************************************

static int32_t AP_handleStartedEvent(void)
{
    Provisioning_AppContext * const stateContext = &apStateContext;
    int32_t retVal = 0;

    /* If provisioning has already started, don't do anything here
     * The state-machine shall keep waiting for the provisioning status
     */
    if (PrvsnStatus_Stopped == stateContext->provisioningStatus)
    {
        SlDeviceVersion_t firmwareVersion = { 0 };

        uint8_t ucConfigOpt = 0;
        uint16_t ucConfigLen = 0;

        /* Get the device's version-information */
        ucConfigOpt = SL_DEVICE_GENERAL_VERSION;
        ucConfigLen = sizeof(firmwareVersion);
        retVal = sl_DeviceGet(SL_DEVICE_GENERAL, &ucConfigOpt, &ucConfigLen,
                              (unsigned char *) (&firmwareVersion));
        ASSERT_ON_ERROR(retVal);

        Display_printf(displayOut, 0, 0,
                       "[apThread] Host Driver Version: %s",
                       SL_DRIVER_VERSION);
        Display_printf(
                displayOut,
                0,
                0,
                "[apThread] Build Version %d.%d.%d.%d.31.%d.%d.%d.%d.%d.%d.%d.%d",
                firmwareVersion.NwpVersion[0], firmwareVersion.NwpVersion[1],
                firmwareVersion.NwpVersion[2], firmwareVersion.NwpVersion[3],
                firmwareVersion.FwVersion[0], firmwareVersion.FwVersion[1],
                firmwareVersion.FwVersion[2], firmwareVersion.FwVersion[3],
                firmwareVersion.PhyVersion[0], firmwareVersion.PhyVersion[1],
                firmwareVersion.PhyVersion[2], firmwareVersion.PhyVersion[3]);

        /* Start provisioning process */
        Display_printf(displayOut, 0, 0, "[apThread] Starting Provisioning - ");
        Display_printf(displayOut, 0, 0,
                       "[apThread] in mode %d (0 = AP, 1 = SC, 2 = AP+SC)",
                       stateContext->provisioningMode);

        retVal = sl_WlanProvisioning(stateContext->provisioningMode, ROLE_STA,
        AP_PROVISIONING_INACTIVITY_TIMEOUT,
                                     NULL, 0);
        ASSERT_ON_ERROR(retVal);

        stateContext->provisioningStatus = PrvsnStatus_InProgress;
        Display_printf(
                displayOut,
                0,
                0,
                "[apThread] Provisioning Started. Waiting to be provisioned!");
    }

    return retVal;
}

//*****************************************************************************
//
//! \brief internal error detection during provisioning process
//!
//! \param[in]  None
//!
//! \return Unexpected event index
//!
//****************************************************************************
static int32_t AP_reportError(void)
{
    Provisioning_AppContext * const stateContext = &apStateContext;
    uint16_t eventIdx = 0;

    for (eventIdx = 0; eventIdx < PrvnEvent_Max; eventIdx++)
    {
        if (0 != (stateContext->pendingEvents & (1 << eventIdx)))
        {
            break;
        }
    }

    Display_printf(displayOut, 0, 0,
                   "[apThread] Unexpected SM: State = %d, Event = %d",
                   stateContext->currentState, eventIdx);

    return eventIdx;
}

//*****************************************************************************
//
//! \brief internal report current state during provisioning process
//!
//! \param[in]  None
//!
//! \return Current state
//!
//****************************************************************************
static int32_t AP_reportSM(void)
{
    Provisioning_AppContext * const stateContext = &apStateContext;
    uint16_t eventIdx = 0;

    for (eventIdx = 0; eventIdx < PrvnEvent_Max; eventIdx++)
    {
        if (0 != (stateContext->pendingEvents & (1 << eventIdx)))
        {
            break;
        }
    }

    if (PrvnEvent_Stopped == eventIdx)
    {
        GPIO_write(Board_LED0, Board_LED_OFF);
    }

    return eventIdx;
}

//*****************************************************************************
//
//! \brief steps following a successful provisioning process
//!
//! \param[in]  None
//!
//! \return 0 on success, negative value otherwise
//!
//****************************************************************************
static int32_t AP_reportSuccess(void)
{
    Provisioning_AppContext * const stateContext = &apStateContext;
    uint16_t ConfigOpt;
    uint16_t ipLen;
    SlNetCfgIpV4Args_t ipV4 = { 0 };
    int32_t retVal;

    Display_printf(displayOut, 0, 0,
                   "[apThread] Provisioning completed successfully..!");
    stateContext->provisioningStatus = PrvsnStatus_Stopped;

    /* Get the device's IP address */
    ipLen = sizeof(SlNetCfgIpV4Args_t);
    ConfigOpt = 0;
    retVal = sl_NetCfgGet(SL_NETCFG_IPV4_STA_ADDR_MODE, &ConfigOpt, &ipLen,
                          (uint8_t *) &ipV4);
    if (retVal == 0)
    {
        Display_printf(displayOut, 0, 0,
                       "[apThread] IP address is %d.%d.%d.%d",
                       SL_IPV4_BYTE(ipV4.Ip, 3), SL_IPV4_BYTE(ipV4.Ip, 2),
                       SL_IPV4_BYTE(ipV4.Ip, 1), SL_IPV4_BYTE(ipV4.Ip, 0));
    }

    GPIO_write(Board_LED0, Board_LED_ON);

    return retVal;
}

void mcuReboot(void)
{
    /* Stop network processor activities before reseting the MCU */
    sl_Stop(SL_STOP_TIMEOUT);

    Display_printf(displayOut, 0, 0, "[Common] CC3220 MCU reset request");

    /* Reset the MCU in order to test the bundle */
    PRCMHibernateCycleTrigger();
}

//*****************************************************************************
//
//! \brief wait for connection following a successful provisioning process
//!
//! \param[in]  None
//!
//! \return 0 on success, negative value otherwise
//!
//****************************************************************************
static int32_t AP_waitForConnection(void)
{
    Provisioning_AppContext * const stateContext = &apStateContext;
    struct timespec ts;
    int32_t retVal;

    while (((!IS_IPV6L_ACQUIRED(Provisioning_ControlBlock.status)
            || !IS_IPV6G_ACQUIRED(Provisioning_ControlBlock.status))
            && !IS_IP_ACQUIRED(Provisioning_ControlBlock.status))
            || !IS_CONNECTED(Provisioning_ControlBlock.status))
    {
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += PROFILE_ASYNC_EVT_TIMEOUT;

        retVal = sem_timedwait(&AP_connectionAsyncSem,
                               &ts);
        /* Freertos return -1 in case of timeout */
        if ((retVal == 116) || (retVal == -1))
        {
            Display_printf(
                    displayOut,
                    0,
                    0,
                    "[apThread] Cannot connect to AP or profile does "
                    "not exist");
            GPIO_write(Board_LED0, Board_LED_OFF);
            /* This state is set so that PrvnEvent_Triggered would invoke
             * provisioning again */
            stateContext->currentState = PrvnState_Init;
            AP_signalProvisioningEvent(PrvnEvent_Triggered);

            return 0;
        }
    }

    Display_printf(displayOut, 0, 0,
                   "[apThread] Connection to AP succeeded");

    return AP_reportSuccess();
}

//*****************************************************************************
//
//! Notify if device return to factory image
//!
//! \param  None
//!
//! \return None
//
//*****************************************************************************
static void AP_notifyReturnToFactoryImage(void)
{
    if (((HWREG(HIB3P3_BASE + 0x00000418) & (1 << 7)) != 0)
            && ((HWREG(0x4402F0C8) & 0x01) != 0))
    {
        Display_printf(
                displayOut, 0, 0,
                "Return To Factory Image successful, Do a power cycle(POR)"
                " of the device using switch SW1-Reset");
        while (1)
            ;
    }
}

//****************************************************************************
//                            MAIN FUNCTION
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
void AP_signalProvisioningEvent(PrvnEvent event)
{
    Provisioning_AppContext * const stateContext = &apStateContext;
    stateContext->pendingEvents |= (1 << event);

}

//*****************************************************************************
//
//! \brief This function gets the current provisioning state
//!
//! \param[in]  None
//!
//! \return Provisioning state
//!
//****************************************************************************
PrvnState AP_getProvisioningState()
{
    Provisioning_AppContext * const stateContext = &apStateContext;

    return stateContext->currentState;
}

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
int32_t AP_provisioningStop(void)
{
    Provisioning_AppContext * const stateContext = &apStateContext;
    int32_t retVal;
    PrvnState provisioningState;
    SlDeviceVersion_t ver = { 0 };
    uint8_t configOpt = 0;
    uint16_t configLen = 0;

    /* Check if provisioning is running */
    configOpt = SL_DEVICE_GENERAL_VERSION;
    configLen = sizeof(ver);
    retVal = sl_DeviceGet(SL_DEVICE_GENERAL, &configOpt, &configLen,
                          (uint8_t *) (&ver));
    if (SL_RET_CODE_PROVISIONING_IN_PROGRESS == retVal)
    {
        Display_printf(
                displayOut,
                0,
                0,
                "[apThread]  Provisioning is already running, stopping it...");
        retVal = sl_WlanProvisioning(SL_WLAN_PROVISIONING_CMD_STOP, ROLE_STA, 0,
        NULL,
                                     0);

        /* Wait for the stopped event to arrive and state change
         * to PrvnState_Init */
        do
        {
            provisioningState = AP_getProvisioningState();
			usleep(1000);
        }
        while (provisioningState != PrvnState_Init);

        stateContext->provisioningStatus = PrvsnStatus_Stopped;

        retVal = SL_RET_CODE_PROVISIONING_IN_PROGRESS;
    }
    else if (retVal < 0)
    {
        return retVal;
    }
    else
    {
        retVal = 0;
    }

    return retVal;
}

static int32_t AP_switchProvisioningMode(void)
{
    Provisioning_AppContext * const stateContext = &apStateContext;
    int32_t retVal = 0;

    /* Switching from BLE mode to AP/SC mode */
    if (Provisioning_ControlBlock.isBleProvisioningActive)
    {
        Display_printf(displayOut, 0, 0,
                       "[apThread] Switching to AP/SC provisioning mode!");
        AP_signalProvisioningEvent(PrvnEvent_Triggered);
    }
    else /* Switching from AP/SC mode to BLE mode*/
    {
        Display_printf(displayOut, 0, 0,
                       "[apThread] Switching to BLE provisioning mode!");

        /* Stop the network processor and update state machine */
        retVal = sl_Stop(SL_STOP_TIMEOUT);
        stateContext->currentState = PrvnState_Init;
    }

    Provisioning_ControlBlock.isBleProvisioningActive =
            !Provisioning_ControlBlock.isBleProvisioningActive;

    return retVal;

}

//*****************************************************************************
//
//! \brief This is the main provisioning task
//!
//! \param[in]  None
//!
//! \return None
//!
//****************************************************************************
void * apThread(void *arg)
{
    int32_t retVal = -1;

    /* Check the wakeup source. If first time entry or wakeup from HIB */
    if (MAP_PRCMSysResetCauseGet() == 0)
    {
        Display_printf(displayOut, 0, 0, "[apThread] Wake up on Power ON");
    }
    else if (MAP_PRCMSysResetCauseGet() == PRCM_HIB_EXIT)
    {
        Display_printf(displayOut, 0, 0,
                       "[apThread] Woken up from Hibernate");
    }

    /* Initialize one-time parameters for AP provisioning */
    AP_provisioningInit();

    /* Configure Provisioning Toggle LED  */
    GPIO_write(Board_LED0, Board_LED_OFF);

    /* Check whether return-to-default occurred */
    AP_notifyReturnToFactoryImage();

    /* At this point, provisioning has not started yet, unless auto
     * provisioning is running. In this case, if provisioning from mobile app
     * is running, it would not be possible to send most of the commands
     * to the device. Need to stop provisioning */
    AP_provisioningStop();

    do
    {
        /* Continue with AP provisioning */
        retVal = AP_provisioningAppTask();

    }
    while (!retVal); /* Exit on failure */

    return (0);
}
//*****************************************************************************
//
// Close the Doxygen group.
//! @}
//
//*****************************************************************************
