#ifndef __FP_FTDI_H__
#define __FP_FTDI_H__
#include "ultis.h"

#define FINGERPRINT_DEV_FILE "/dev/inv_spi"

#define ABS_MAX_TRANSACTION_SIZE	2048
#define _max_transaction_size		1024
#define READ_BIT			0x80
static void showVersion(DWORD locationId);
int FTDI_spi_reg_read(uint8_t reg, uint8_t * rbuffer, uint32_t rlen);
int FTDI_spi_reg_write(uint8_t reg, const uint8_t * wbuffer, uint32_t wlen);
int inv_spi_init();

#endif
