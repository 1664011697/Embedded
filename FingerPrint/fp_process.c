#include "fp_process.h"
#include "fp_FTDI.h"
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond  = PTHREAD_COND_INITIALIZER;
static fingerprint_device_t *device_p;
static fingerprint_error_t fp_error = 0;
static bool need_cancel = false;
static int spi_file_id = -1;
static gid_t gid=0xffffffff;
static int timeout=0;
static fingerprint_finger_id_t *fids = NULL;
static int n_fids = -1;
static const fingerprint_module_t *fp_module_p=NULL;
static volatile bool change_flag=false;
static int auth_flag=8;
static Mode_t run_mode = WAIT_MODE;
static inline int fingerprint_open(const fingerprint_module_t* module,
                                   fingerprint_device_t** device) {
    return module->common.methods->open(&module->common, NULL , (hw_device_t**)device);
}
static inline int fingerprint_close(fingerprint_device_t* device) {
    return device->common.close(&device->common);
}
static int fingerprint_load(const fingerprint_module_t** pHmi, char *lib)
{
    int status = -EINVAL;
    fingerprint_module_t *hmi = NULL;
    void* handle;
    ALOGI("Load HAL module %s\n", lib);
    handle = dlopen(lib, RTLD_NOW); //RTLD_LAZY
    if (handle == NULL) {
        char const *err_str = dlerror();
        out("load: module=%s\n%s\n", lib, err_str?err_str:"unknown");
        status = -EINVAL;
        goto done;
    }
    /* Get the address of the struct hal_module_info. */
    hmi = (fingerprint_module_t *)dlsym(handle, HAL_MODULE_INFO_SYM_AS_STR);
    if (hmi == NULL) {
        out("load: couldn't find symbol " HAL_MODULE_INFO_SYM_AS_STR"\n");
        status = -EINVAL;
        goto done;
    }
    /* Check that the id matches */
    if (strcmp(FINGERPRINT_HARDWARE_MODULE_ID, hmi->common.id) != 0) {
        out("load: id=%s != fp_module->id=%s\n", FINGERPRINT_HARDWARE_MODULE_ID, hmi->common.id);
        status = -EINVAL;
        goto done;
    }
    out("load name moduel=%s\n", hmi->common.name);
    hmi->common.dso = handle;
    /* success */
    status = 0;
done:
    if (status != 0) {
        hmi = NULL;
        if (handle != NULL) {
            dlclose(handle);
            handle = NULL;
        }
    } else {
        ALOGV("loaded HAL id=%s path=%s hmi=%p handle=%p\n",
              FINGERPRINT_HARDWARE_MODULE_ID, lib, *pHmi, handle);
    }
    *pHmi = hmi;
    return status;
}

static void fingerprint_unload(const fingerprint_module_t *hmi)
{
    dlclose(hmi->common.dso);
}
const char * str_hal_acquired_info(fingerprint_acquired_info_t info)
{
    switch (info) {
    case FINGERPRINT_ACQUIRED_GOOD:
        return "good";
    case FINGERPRINT_ACQUIRED_PARTIAL:
        return "partial (sensor needs more data, i.e. longer swipe)";
    case FINGERPRINT_ACQUIRED_INSUFFICIENT:
        return "insufficient (image doesn't contain enough detail for recognition)";
    case FINGERPRINT_ACQUIRED_IMAGER_DIRTY:
        return "imager dirty (sensor needs to be cleaned)";
    case FINGERPRINT_ACQUIRED_TOO_SLOW:
        return "too slow (mostly swipe-type sensors; not enough data collected)";
    case FINGERPRINT_ACQUIRED_TOO_FAST:
        return "too fast (for swipe and area sensors; tell user to slow down)";
    default:
        return "unkown";
    }
}
void authFlag_reset()
{
    auth_flag=6;
}
void fp_close();
static bool cancel_flag=false;
int fp_cancel()
{
    int ret = 0;
    if((run_mode != WAIT_MODE))
    {
        pthread_cond_signal(&cond);
        cancel_flag=true;
        while(run_mode != WAIT_MODE);
        out("change success\n");
        return 1;
    }
    return -1;
}
bool necessaryTo_switch()
{
    return run_mode != WAIT_MODE;
}
Mode_t currMode_get()
{
    return run_mode;
}
static void fingerprint_notify(const fingerprint_msg_t *msg)
{
    switch ((int)msg->type) {
    case FINGERPRINT_ERROR:
        ALOGE("Notify - Error: %d\n", msg->data.error);
        fp_error = msg->data.error;
        pthread_cond_signal(&cond);
        break;

    case FINGERPRINT_ACQUIRED:
        ALOGD("Notify - Acquired: %d\n", msg->data.acquired.acquired_info);
        if (msg->data.acquired.acquired_info != FINGERPRINT_ACQUIRED_GOOD)
        {
            Send_commond(FINGERPRINT_AUTH_ERROR);
            if(run_mode == AUTH_MODE)
            {
                out("auth_mode.flag:%u\n",auth_flag);
                if(auth_flag) --auth_flag;
                if(auth_flag == 0){
                    pthread_cond_signal(&cond);
                    out("cancel.\n");
                }
            }
        }
        break;

    case FINGERPRINT_AUTHENTICATED:
        ALOGD("Notify - Authenticated: fid=%x, gid=%x\n",
              msg->data.authenticated.finger.fid,
              msg->data.authenticated.finger.gid);
        if (msg->data.authenticated.finger.fid != 0) 
        {
            pthread_cond_signal(&cond);
            Send_commond(FINGERPRINT_AUTH_FINISH);
        }
        else
        {
            out("one image.\n");

            Send_commond(FINGERPRINT_AUTH_ERROR);
            out("auth_mode.flag:%u\n",auth_flag);
            if(auth_flag) --auth_flag;
            if(auth_flag == 0){
                pthread_cond_signal(&cond);
                out("cancel.\n");
            }
            //Send_commond(FINGERPRINT_AUTH_ERROR);
        }
        break;

    case FINGERPRINT_TEMPLATE_ENROLLING:
        ALOGD("Notify - EnrollResult: fid=%x, gid=%x, remaining=%d\n",
              msg->data.enroll.finger.fid,
              msg->data.enroll.finger.gid,
              msg->data.enroll.samples_remaining);
        if (msg->data.enroll.samples_remaining == 0)
        {
            pthread_mutex_lock(&lock);
            pthread_cond_signal(&cond);
            pthread_mutex_unlock(&lock);
            Send_commond(FINGERPRINT_ENROLL_FINISH);
        }
        else
        {
            out("One finger image captured, still wait for %d captures\n", msg->data.enroll.samples_remaining);
            Send_commond(FINGERPRINT_ENROLL_GOOD);
        }
        break;

    case FINGERPRINT_TEMPLATE_REMOVED:
        ALOGD("Notify - remove: fid=%x, gid=%x\n",
              msg->data.removed.finger.fid,
              msg->data.removed.finger.gid);
        break;
    default:
        ALOGE("invalid msg type: %d\n", msg->type);
        return;
    }
}
const char * str_halerror(fingerprint_error_t err)
{
    switch (err) {
    case FINGERPRINT_ERROR_HW_UNAVAILABLE:
        return "ERROR_HW_UNAVAILABLE";
    case FINGERPRINT_ERROR_UNABLE_TO_PROCESS:
        return "ERROR_UNABLE_TO_PROCESS";
    case FINGERPRINT_ERROR_TIMEOUT:
        return "ERROR_TIMEOUT";
    case FINGERPRINT_ERROR_NO_SPACE:
        return "ERROR_NO_SPACE";
    case FINGERPRINT_ERROR_CANCELED:
        return "ERROR_CANCELED";
    case FINGERPRINT_ERROR_UNABLE_TO_REMOVE:
        return "ERROR_UNABLE_TO_REMOVE";
    default:
        return "UNKOWN";
    }
}
void handle_signal(int __unused signal)
{
    int s = 0;

    if (need_cancel) {
        s = device_p->cancel(device_p);
        if (s)
            out("Failed to cancel %d (%s)\n", s, strerror(s));
    }
    fingerprint_close(device_p);
    out("close success\n");
    exit(s);
}
void fp_spi_init()
{
    inv_spi_init();
}
void fp_close()
{
    if(need_cancel)
    {
        out("need_cancel..before.\n");
        device_p->cancel(device_p);
        need_cancel=false;
        out("need_cancel..end.\n");
    }
    if(!cancel_flag)
        pthread_cond_signal(&cond);
    fingerprint_close(device_p);
}
int fingerprint_init(char *fp_lib)
{
    int ret;
    /* Init test */
    if (fp_lib != NULL)
        ret = fingerprint_load(&fp_module_p, fp_lib);
    else{
        return -1;
    }
    out("fp_module_p loading %x\n",fp_module_p);
    if (ret)
        exit(EXIT_FAILURE);

    /* Init fingerprint HAL */
    ret = fingerprint_open(fp_module_p, &device_p);
    if (ret)
    {
        out("\nFailed to open fingerprint module\n");
        goto unload;
    }
    ret = device_p->set_notify(device_p, fingerprint_notify);
    if (ret)
    {
        out("\nFailed register callback\n");
        goto exit;
    }
    ret = device_p->set_spi_read(device_p, FTDI_spi_reg_read);
    if (ret)
    {
        out("\nFailed register callback spi read\n");
        goto exit;
    }
    ret = device_p->set_spi_write(device_p, FTDI_spi_reg_write);
    if (ret)
    {
        out("\nFailed register callback spi write\n");
        goto exit;
    }
    out("device_p->init start 0x%x\n",device_p);
    ret = device_p->init(device_p);
    if (ret)
    {
        out("\nFailed to do init\n");
        goto exit;
    }
    /*
       struct sigaction sa = {
       .sa_handler = &handle_signal,
       };
       sigfillset(&sa.sa_mask);
       if (sigaction(SIGINT, &sa, NULL) == -1) {
       perror("Error: cannot handle SIGINT"); // Should not happen
       }
       if (sigaction(SIGQUIT, &sa, NULL) == -1) {
       perror("Error: cannot handle SIGQUIT"); // Should not happen
       }
       */
    return 1;
exit:
    exit(-1);
unload:
    fingerprint_unload(fp_module_p);
    exit(0);
}
void fingerprint_enroll()
{
    run_mode = ENROLL_MODE;
    Send_commond(FINGERPRINT_ENROLL_BEGIN);
    int ret;
    hw_auth_token_t hat = {
        .version = 0,
        .user_id = 1234,
        .authenticator_id = 0,
        .authenticator_type = HW_AUTH_FINGERPRINT,
        .timestamp = 0,
        .hmac = { 0 },
    };
    out("\nFingerprint pre_enroll: start\n");
    struct timespec ts;
    hat.challenge = device_p->pre_enroll(device_p);
    if (hat.challenge == 0)
    {
        out("\nFailed to pre_enroll\n");
        goto exit;
    }

    out("\nFingerprint enroll: start\n");
    need_cancel = true;
    ret = device_p->enroll(device_p, &hat, gid, timeout ? timeout : 600); /* 10 min. time-out */
    if (ret)
    {
        out("\n Failed to start enrollment\n");
        goto exit;
    }

    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += (timeout ? timeout+60 : 11*60);
    pthread_mutex_lock(&lock);
    ret = pthread_cond_timedwait(&cond, &lock, &ts);
    pthread_mutex_unlock(&lock);
    out("end.\n");
    if (ret)
    {
        int s;
        out("\n Failed to enroll - pthread_cond_wait returned %d (%s)\n", ret, strerror(ret));
        s = device_p->cancel(device_p);
        need_cancel = false;
        if (s)
            out("\n Failed to cancel %d (%s)\n", s, strerror(s));
        run_mode = WAIT_MODE;
        goto exit;
    }
    else if (fp_error)
    {
        out("\n Failed to enroll - error reported by HAL %d (%s)\n", fp_error, str_halerror(fp_error));
        goto exit;
    }
    out("\n Fingerprint enroll: all captures done\n");
    ret=device_p->post_enroll(device_p);
    if(ret)
    {
        out("\n Failed to end enrollment\n");
        goto exit;
    }
    run_mode = WAIT_MODE;
    return;
exit:
    return;
}
void fingerprint_clear()
{
    int ret;
    unsigned size = n_fids;
    run_mode = CLEAR_MODE;
    fids=calloc(sizeof(*fids),10);
    if (fids == NULL) {
        out("Failed to allocate memory\n");
        goto exit;
    }
    ret = device_p->enumerate(device_p, fids, &size);
    if (ret < 0) {
        out("Failed to enumerate list of registered finger\n");
        goto exit;
    }
    if ((unsigned)ret != size) {
        void *ptr;
        ptr = realloc(fids, sizeof(*fids)*ret);
        if (ptr == NULL) {
            out("Failed to allocate memory\n");
            goto exit;
        }
        fids = ptr;
        ret = device_p->enumerate(device_p, fids, &size);
        if (ret < 0) {
            out("Failed to enumerate list of registered finger\n");
            goto exit;
        }
    }
    n_fids = size;
    int i;
    out("List of registered fingers\n");
    for (i=0; i<n_fids; i++)
        out("%d: %x - %x\n", i, fids[i].fid, fids[i].gid);
    for(i=0;i<n_fids;i++)
        ret = device_p->remove(device_p, fids[i].gid, fids[i].fid);
    out("Fingerprint successfully removed\n");
    Send_commond(FINGERPRINT_CLEAR);
    run_mode = WAIT_MODE;
    return ;
exit:
    run_mode = WAIT_MODE;
    return;
}
void fingerprint_auth()
{
    run_mode = AUTH_MODE;
    Send_commond(FINGERPRINT_AUTH_BEGIN);
    int ret;
    struct timespec ts;
    if (fp_error)
    {
        out("Error reported by HAL before authenticate: %d (%s)\n", fp_error, str_halerror(fp_error));
        goto exit;
    }
    out("auth begin.\n");
    ret = device_p->authenticate(device_p, 0, gid);
    if (ret) {
        out("\n Failed to start authentication\n");
        goto exit;
    }
    out("\n Fingerprint authenticate: start\n");

    need_cancel = true;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += (timeout ? timeout : 2*60);
    pthread_mutex_lock(&lock);
    out("begin\n");
    ret = pthread_cond_timedwait(&cond, &lock, &ts);
    out("end.\n");
    pthread_mutex_unlock(&lock);
    if (ret)
    {
        int s;
        out("Failed to authenticate - pthread_cond_wait returned %d (%s)\n", ret, strerror(ret));
        s = device_p->cancel(device_p);
        need_cancel = false;
        if (s)
            out("Failed to cancel %d (%s)\n", s, strerror(s));
        goto exit;
    }
    else if (fp_error)
    {
        out("Failed to authenticate - error sent from HAL %d\n", fp_error);
        goto exit;
    }
    out("\n Fingerprint authenticate: Success!!!\n");
    run_mode = WAIT_MODE;
    return ;
exit:
    run_mode = WAIT_MODE;
    return;

}

