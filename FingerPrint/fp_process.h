#ifndef __FP_PROCESS_H__
#define __FP_PROCESS_H__

#include "ultis.h"


#define FINGERPRINT_CMD_WRITE_REG         100
#define FINGERPRINT_CMD_READ_REG          101
#define FINGERPRINT_CMD_SET_REG_ADDR      102
#define FINGERPRINT_CMD_SET_LEN           103

void authFlag_reset();
int fp_cancel();
bool necessaryTo_switch();
Mode_t currMode_get();
int fingerprint_init(char *fp_lib);
void fingerprint_enroll();
void fingerprint_clear();
void fingerprint_auth();

#endif
