#ifndef __ULTIS_H__
#define __ULTIS_H__


#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <dlfcn.h>
#include <pthread.h>
#include <string.h>
#include <getopt.h>
#include <signal.h>
#include <dlfcn.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <unistd.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <hardware/hardware.h>
#include <hardware/fingerprint.h>
#include <hardware/hw_auth_token.h>
//#include "hardware/hardware.h"
//#include "hardware/fingerprint.h"
#include "ftd2xx.h"
#include "libft4222.h"
#include "crc.h"

#define ALOGI printf
#define ALOGE printf
#ifndef ALOGD
#define ALOGD printf
#endif
#define ALOGV printf
#ifndef __unused
#define __unused
#endif
#define FINGERPRINT_NAME "fingerprint"
#define TDK_FP_MODULE_PATH "/system/lib64/hw/tdk_fp.so"
#define out(args...) do { printf(args); fflush(stdout); } while (0)
#define SLAVE_SELECT(x) (1 << (x))

typedef enum fp_msgType_fromIoc
{
    SPI_FINGERPRINT_NONE=1,
    SPI_FINGERPRINT_ENROLLING,
    SPI_FINGERPRINT_AUTHENTICATED,
    SPI_FINGERPRINT_CLEAR
}fp_msgType_fromIoc_t;
typedef enum fingerprint_info {
    FINGERPRINT_ENROLL_BEGIN = 6,
    FINGERPRINT_AUTH_BEGIN = 7,
    FINGERPRINT_ENROLL_FINISH = 8,
    FINGERPRINT_AUTH_FINISH = 9,
    FINGERPRINT_ENROLL_GOOD=10,

    FINGERPRINT_AUTH_ERROR = 11,
    FINGERPRINT_CLEAR=12,
    FINGERPRINT_ACQUIRED_NONE=13
} fingerprint_info_t;

typedef enum 
{
    WAIT_MODE=0,
    AUTH_MODE,
    ENROLL_MODE,
    CLEAR_MODE,
}Mode_t;
static void pabort(const char *s)
{
    perror(s);
    abort();
}

#endif
