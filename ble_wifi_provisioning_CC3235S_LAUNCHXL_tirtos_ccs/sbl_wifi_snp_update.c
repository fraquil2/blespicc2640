/*
 * Copyright (c) 2016-2017, Texas Instruments Incorporated
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
#include <ti/drivers/net/wifi/simplelink.h>
#include <ti/sbl/sbl.h>
#include <ti/sbl/sbl_cmd.h>
#include <ti/sap/snp.h>
#include "sbl_wifi_snp_update.h"

int32_t WiFi_SNP_updateFirmware(SBL_Params* sblParams)
{
    int32_t resCode;
    int32_t devHandle;
    uint32_t imgSize;
    uint32_t bytesWritten;
    uint32_t transLen;
    int32_t returnCode = WIFI_SNP_SBL_SUCCESS;
    uint8_t transferBuffer[SBL_MAX_TRANSFER];
    SlFsFileInfo_t pFsFileInfo;

    /* Opening the SBL Target */
    if ((SBL_open(sblParams) != SBL_SUCCESS)
            || (SBL_openTarget() != SBL_SUCCESS))
    {
        return WIFI_SNP_SBL_SBL_ERROR;
    }

    /* Starting the NWP */
    resCode = sl_Start(0, 0, 0);

    if (resCode < 0)
    {
        returnCode = WIFI_SNP_SBL_SNP_ERROR;
        goto ReturnPoint;
    }

    /* Seeing if the firmware image exists in the new image  */
    resCode = sl_FsGetInfo(SNP_FIRMWARE_PATH, 0, &pFsFileInfo);

    if (resCode < 0)
    {
        returnCode = WIFI_SNP_SBL_FILE_ERROR;
        goto ReturnPoint;
    }

    /* Opening the firmware image and writing to the flash in chunks */
    devHandle = sl_FsOpen(SNP_FIRMWARE_PATH, SL_FS_READ, 0);

    if (devHandle < 0)
    {
        returnCode = WIFI_SNP_SBL_FILE_ERROR;
        goto ReturnPoint;
    }

    /* Writing the new image */
    imgSize = pFsFileInfo.Len;
    bytesWritten = 0;

    /* Check if the last sector is protected. If it is do not write the last
     * sector. */
    if (SBL_isLastSectorProtected() == true)
    {
        imgSize -= SBL_PAGE_SIZE;
    }

    /* Erase flash  and invoke SBL */
    if (SBL_eraseFlash(SNP_IMAGE_START, imgSize) != SBL_SUCCESS)
    {
        returnCode = WIFI_SNP_SBL_TRANSFER_ERROR;
        goto ReturnPoint;
    }

    /* Invoking the SBL */
    SBL_CMD_download(SNP_IMAGE_START, imgSize);

    /* Verify target status after invoking SBL */
    if (SBL_CMD_getStatus() != SBL_CMD_RET_SUCCESS)
    {
        returnCode = WIFI_SNP_SBL_TRANSFER_ERROR;
        goto ReturnPoint;
    }

    /* Writing the image */
    while (bytesWritten < imgSize)
    {
        /* Determine number of bytes in this transfer */
        transLen =
                (SBL_MAX_TRANSFER < (imgSize - bytesWritten)) ?
                        SBL_MAX_TRANSFER : (imgSize - bytesWritten);

        /* Read bytes of image */
        resCode = sl_FsRead(devHandle, bytesWritten,
                            (unsigned char *) transferBuffer, transLen);

        /* Send data to target */
        SBL_CMD_sendData(transferBuffer, transLen);

        /* Verify Device Status after download command */
        if (SBL_CMD_getStatus() != SBL_CMD_RET_SUCCESS)
        {
            returnCode = WIFI_SNP_SBL_TRANSFER_ERROR;
            goto ReturnPoint;
        }

        /* Update bytes written and image address of next read */
        bytesWritten += transLen;
    }

    /* Closing out the file handle and the NWP */
    sl_FsClose(devHandle, NULL, NULL, 0);

    /* Making sure that our bytes match up */
    if (bytesWritten != pFsFileInfo.Len)
    {
        returnCode = WIFI_SNP_SBL_TRANSFER_ERROR;
        goto ReturnPoint;
    }

    ReturnPoint:
    /* Send the reset command */
    SBL_CMD_reset();

    /* Reset target and exit SBL code */
    SBL_closeTarget();
    SBL_close();

    /* Stopping the NWP */
    if(devHandle != NULL)
    {
        sl_FsClose(devHandle, NULL, NULL, 0);
    }
    sl_Stop(0);

    return returnCode;

}
