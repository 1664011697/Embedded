#include "fp_commond.h"

static Spibuf_t buf;
static int fd=-1;
static SpiMsg_t receive_msg;
static SpiMsg_t response_msg;
static const char *device = "/dev/spidev2.0";
static uint8_t *pType=NULL;
static struct spi_ioc_transfer tr;
static int delay_flag=2;
uint8_t Recv_commond()
{
    return receive_msg._type;
}
void Send_commond(uint8_t cmd)
{
    response_msg._type=cmd;
}
void spiTx_enpacket()
{
    *pType=response_msg._type;
}
void spiRx_unpacket()
{
    int ret;
    T_IPC_DATA *pmsg;
    pmsg=(T_IPC_DATA*)buf._rbuf;
    uint8_t *data=pmsg->pData;
    uint16_t FeatureSize=*((uint16_t*)data);
    T_IPC_HEADER *FeatureHead=(T_IPC_HEADER*)(data+(sizeof(uint16_t)));
    uint8_t *pFeatureData=((uint8_t*)(data+(sizeof(T_IPC_HEADER)*FeatureSize+sizeof(uint16_t))));
    for(ret=0;ret<FeatureSize;ret++,FeatureHead++)
    {
        if(FeatureHead->Id==FINGERPRINT_ID)
        {
            SpiMsg_t *msg=(SpiMsg_t*)(pFeatureData+FeatureHead->Offset);
            receive_msg._type=msg->_type;
            break;
        }
    }
}
void Transfer()
{
    int ret;
    usleep(10000);
    spiTx_enpacket();
    ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
    if (ret < 1)
    {
        pabort("can't send spi message");
        return ;
    }
    if(delay_flag) --delay_flag;
    else
    {
        response_msg._type = FINGERPRINT_ACQUIRED_NONE;
        delay_flag=2;
    }
    spiRx_unpacket();
}
void Spi_init()
{
    unsigned char mode=0,bits=8;
    int speed=4000000;
    int ret;
    fd = open(device, O_RDWR);
    if (fd < 0)
        pabort("can't open device");
    /*
     * spi mode
     */
    ret = ioctl(fd, SPI_IOC_WR_MODE, &mode);
    if (ret == -1)
        pabort("can't set spi mode");

    ret = ioctl(fd, SPI_IOC_RD_MODE, &mode);
    if (ret == -1)
        pabort("can't get spi mode");
    /*
     * bits per word
     */
    ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
    if (ret == -1)
        pabort("can't set bits per word");

    ret = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
    if (ret == -1)
        pabort("can't get bits per word");
    /*
     * max speed hz
     */
    ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
    if (ret == -1)
        pabort("can't set max speed hz");

    ret = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
    if (ret == -1)
        pabort("can't get max speed hz");

    out("spi mode: %d\n", mode);
    out("bits per word: %d\n", bits);
    out("max speed: %d Hz (%d KHz)\n", speed, speed/1000);
    tr.tx_buf = (unsigned long)buf._tbuf;
    tr.rx_buf = (unsigned long)buf._rbuf;
    tr.len = SPI_BUF_SIZE;
    tr.delay_usecs = 0;
    tr.speed_hz = speed;
    tr.bits_per_word = 8;

    response_msg._type=FINGERPRINT_ACQUIRED_NONE;
    receive_msg._type=SPI_FINGERPRINT_NONE;
    Spibuf_init();
}
void Spibuf_init()
{
    memset(buf._tbuf,0,SPI_BUF_SIZE);
    memset(buf._rbuf,0,SPI_BUF_SIZE);
    T_IPC_DATA *pmsg=(T_IPC_DATA*)buf._tbuf;
    uint8_t *FeatureData = pmsg->pData;
    pmsg->Header32Bit=IPC_SEND_HEADER;
    pmsg->CounterNumber=0;
    pmsg->Size16Bit=sizeof(SpiMsg_t)+sizeof(T_IPC_HEADER)+sizeof(uint16_t);
    pmsg->ProtocolVersion=IPC_PROTOCOL_VERSION;

    T_IPC_HEADER *FeatureHead = (T_IPC_HEADER*)(FeatureData+sizeof(uint16_t));
    uint16_t FeatureSize=1;
    *((uint16_t*)FeatureData)=FeatureSize;
    FeatureData = FeatureData+sizeof(uint16_t)+sizeof(T_IPC_HEADER)*FeatureSize;
    FeatureHead->Id=FINGERPRINT_ID;
    FeatureHead->Offset=0;
    FeatureHead->Size=sizeof(SpiMsg_t);
    SpiMsg_t *msg=(SpiMsg_t*)FeatureData;
    msg->_type=FINGERPRINT_ACQUIRED_NONE;
    pType=&(msg->_type);
}
