#include "fp_FTDI.h"

static FT_HANDLE ftHandle = (FT_HANDLE)NULL;
static void showVersion(DWORD locationId)
{
    FT_STATUS            ftStatus;
    FT4222_STATUS        ft4222Status;
    FT4222_Version       ft4222Version;
    ftStatus = FT_OpenEx((PVOID)(uintptr_t)locationId, 
                         FT_OPEN_BY_LOCATION, 
                         &ftHandle);
    if (ftStatus != FT_OK)
    {
        out("FT_OpenEx failed (error %d)\n", 
               (int)ftStatus);
        return;
    }
    // Get version of library and chip.    
    ft4222Status = FT4222_GetVersion(ftHandle,
                                     &ft4222Version);
    if (FT4222_OK != ft4222Status)
    {
        out("FT4222_GetVersion failed (error %d)\n",
               (int)ft4222Status);
    }
    else
    {
        out("  Chip version: %08X, LibFT4222 version: %08X\n",
               (unsigned int)ft4222Version.chipVersion,
               (unsigned int)ft4222Version.dllVersion);
    }
}
int FTDI_spi_reg_read(uint8_t reg, uint8_t * rbuffer, uint32_t rlen)
{
    int rc;
    uint16_t size_transferred = 0;
    /* looks like it is safe to to use same buffer for read/write payload but this may not
       true with another SPI configuration, so use two buffers for read and write payload... */
    uint8_t payload_buffer_w[ABS_MAX_TRANSACTION_SIZE]; /* only the first byte is used */
    uint8_t payload_buffer_r[ABS_MAX_TRANSACTION_SIZE];

    if(rlen > (_max_transaction_size - 1U)) {
        out("ft4222 size error\n");
    }

    payload_buffer_w[0] = reg | READ_BIT;  /* set MBS to indicate read register request */
    memset(&payload_buffer_w[1], 0, rlen); /* clear write buffer to avoid sending garbage while reading */
    rc = FT4222_SPIMaster_SingleReadWrite(ftHandle, payload_buffer_r,
                                          payload_buffer_w, (rlen+1), &size_transferred, 1);

    if (rc != FT4222_OK || size_transferred != (rlen +1)) {
        out("FT4222: read_reg() = %d - fail to read %d bytes from reg %x (transferred: %d/%d)\n",
               rc, rlen, reg, size_transferred, rlen + 1);
        return FT4222_IO_ERROR;
    }
    /* copy read data to output buffer */
    memcpy(rbuffer, &payload_buffer_r[1], rlen);
    return FT4222_IO_ERROR;
}

int FTDI_spi_reg_write(uint8_t reg, const uint8_t * wbuffer, uint32_t wlen)
{
    int rc;
    uint16_t size_transferred = 0;
    uint8_t payload_buffer_w[ABS_MAX_TRANSACTION_SIZE];

    if(wlen > (_max_transaction_size - 1U)) {
        out("ft4222 size error");
    }

    payload_buffer_w[0] = reg;
    memcpy(&payload_buffer_w[1], wbuffer, wlen);

    rc = FT4222_SPIMaster_SingleWrite(ftHandle, payload_buffer_w,
                                      wlen + 1, &size_transferred, 1);

    if (rc != FT4222_OK || size_transferred != (wlen + 1)) {
        out("FT4222: write_reg() = %d - fail to write %d bytes to reg %x (transferred: %d/%d)\n",
               rc, wlen, reg, size_transferred, wlen + 1);
        return FT4222_IO_ERROR;
    }

    return FT4222_IO_ERROR;
}
int inv_spi_init() 
{
    FT_STATUS                 ftStatus;
    FT_DEVICE_LIST_INFO_NODE *devInfo = NULL;
    DWORD                     numDevs = 0;
    int                       i;
    int                       retCode = 0;
    int                       found4222 = 0;

    ftStatus = FT_CreateDeviceInfoList(&numDevs);
    if (ftStatus != FT_OK) 
    {
        out("FT_CreateDeviceInfoList failed (error code %d)\n", 
               (int)ftStatus);
        retCode = -10;
        goto exit;
    }

    if (numDevs == 0)
    {
        out("No devices connected.\n");
        retCode = -20;
        goto exit;
    }

    /* Allocate storage */
    out("numDevs:%d\n",numDevs);
    devInfo = calloc((size_t)numDevs,
                     sizeof(FT_DEVICE_LIST_INFO_NODE));
    if (devInfo == NULL)
    {
        out("Allocation failure.\n");
        retCode = -30;
        goto exit;
    }

    /* Populate the list of info nodes */
    ftStatus = FT_GetDeviceInfoList(devInfo, &numDevs);
    if (ftStatus != FT_OK)
    {
        out("FT_GetDeviceInfoList failed (error code %d)\n",
               (int)ftStatus);
        retCode = -40;
        goto exit;
    }

    for (i = 0; i < (int)numDevs; i++) 
    {
        if (devInfo[i].Type == FT_DEVICE_4222H_0  ||
            devInfo[i].Type == FT_DEVICE_4222H_1_2)
        {
            // In mode 0, the FT4222H presents two interfaces: A and B.
            // In modes 1 and 2, it presents four interfaces: A, B, C and D.

            size_t descLen = strlen(devInfo[i].Description);

            if ('A' == devInfo[i].Description[descLen - 1])
            {
                // Interface A may be configured as an I2C master.
                out("\nDevice %d: '%s'\n",
                       i,
                       devInfo[i].Description);
                showVersion(devInfo[i].LocId);
            }
            else
            {
                // Interface B, C or D.
                // No need to repeat version info of same chip.
            }

            found4222++;
        }

        if (devInfo[i].Type == FT_DEVICE_4222H_3)
        {
            // In mode 3, the FT4222H presents a single interface.  
            out("\nDevice %d: '%s'\n",
                   i,
                   devInfo[i].Description);
            showVersion(devInfo[i].LocId);

            found4222++;
        }
    }

    if (found4222 == 0)
        out("No FT4222H detected.\n");

    // Configure the FT4222 as an SPI Master.
    ftStatus = FT4222_SPIMaster_Init(
                                     ftHandle, 
                                     SPI_IO_SINGLE, // 1 channel
                                     CLK_DIV_8, // 60 MHz / 8 == 7.5 MHz
                                     CLK_IDLE_LOW, // clock idles at logic 0
                                     CLK_LEADING, // data captured on rising edge
                                     SLAVE_SELECT(0)); // Use SS0O for slave-select
    if (FT4222_OK != ftStatus)
    {
        out("FT4222_SPIMaster_Init failed (error %d)\n",
               (int)ftStatus);
        goto exit;
    }

    ftStatus = FT4222_SPI_SetDrivingStrength(ftHandle,
                                             DS_8MA,
                                             DS_8MA,
                                             DS_8MA);
    if (FT4222_OK != ftStatus)
    {
        out("FT4222_SPI_SetDrivingStrength failed (error %d)\n",
               (int)ftStatus);
        goto exit;
    }

    return 1;
exit:
    free(devInfo);
    return retCode;
}
