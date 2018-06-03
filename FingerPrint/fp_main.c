#include "fp_commond.h"
#include "fp_FTDI.h"
#include "fp_process.h"
#include "ultis.h"
static volatile uint8_t active_cmd = SPI_FINGERPRINT_NONE;
static volatile uint8_t change_cmd = SPI_FINGERPRINT_NONE;
static pthread_mutex_t slock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t scond  = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t proc_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t proc_cond  = PTHREAD_COND_INITIALIZER;
void* Transfer_Thread(void* arg)
{
    while(1)
    {
        Transfer();
        change_cmd = Recv_commond();
        if(change_cmd != SPI_FINGERPRINT_NONE)
        {
            if(active_cmd == SPI_FINGERPRINT_NONE)
            {
                pthread_mutex_lock(&proc_lock);
                pthread_cond_signal(&proc_cond);
                pthread_mutex_unlock(&proc_lock);
            }
            else if(change_cmd != active_cmd)
            {
                pthread_mutex_lock(&slock);
                pthread_cond_signal(&scond);
                pthread_mutex_unlock(&slock);
            }
        }
    }
}
int fp_Modeswitch()
{
    int ret;
    if(necessaryTo_switch())
    {
        ret = fp_cancel();
    }
    return ret;
}
void* fpProcess_Thread(void* arg)
{
    Mode_t currmode;
    while(1)
    {
        pthread_mutex_lock(&proc_lock);
        pthread_cond_wait(&proc_cond,&proc_lock);
        pthread_mutex_unlock(&proc_lock);
        switch(active_cmd)
        {
        case SPI_FINGERPRINT_AUTHENTICATED:
            fingerprint_auth();
            break;
        case SPI_FINGERPRINT_ENROLLING:
            fingerprint_enroll();
            break;
        case SPI_FINGERPRINT_CLEAR:
            fingerprint_clear();
            break;
        case SPI_FINGERPRINT_NONE:
        default:
            break;
        }
        active_cmd = SPI_FINGERPRINT_NONE;
    }
}
int main()
{
    Spi_init();
    char *lib = TDK_FP_MODULE_PATH;
    fingerprint_init(lib);
    pthread_t thread1,thread2;
    pthread_create(&thread1,NULL,Transfer_Thread,NULL);
    pthread_create(&thread2,NULL,fpProcess_Thread,NULL);
    int ret;
    while(1)
    {
        pthread_mutex_lock(&slock);
        pthread_cond_wait(&scond,&slock);
        pthread_mutex_unlock(&slock);
        ret = fp_Modeswitch();
        if(ret == -1)
        {
            out("failed to cancel!\n");
            exit(-1);
        }
        pthread_mutex_lock(&slock);
        pthread_cond_signal(&proc_cond);
        pthread_mutex_unlock(&slock);
    }
    pthread_join(thread1,NULL);
    pthread_join(thread2,NULL);
    return 1;
}

