#ifndef __FP_COMMOND_H__
#define __FP_COMMOND_H__

#include "ultis.h"
#define IPC_GATEWAY_MAGIC_CODE (0x20160315)
#define IPC_RECV_HEADER_CRC_ERROR (0x5A5A5A5A)
#define IPC_RECV_HEADER (0xA5A5A5A5)
#define IPC_SEND_HEADER (0xA5A5A5A5)
#define IPC_CRC_LEN 2

#define IPC_MESSAGE_HEADER_SIZE 16
#define IPC_MESSAGE_LENGTH 512
#define IPC_PROTOCOL_VERSION    0x00u
#define IPC_MESSAGE_DATA_SIZE (IPC_MESSAGE_LENGTH - IPC_MESSAGE_HEADER_SIZE)

typedef struct T_IPC_DATA_MESSAGE {
    unsigned int   Header32Bit; // 0xA5A5A5A5 is Data, 0x5A5A5A5A is CRC error.
    unsigned int   CounterNumber;
    unsigned short   Size16Bit;
    unsigned short   ProtocolVersion;
    unsigned char   CID8Bit;
    unsigned char   SID8Bit;
    unsigned char   pData[IPC_MESSAGE_DATA_SIZE];
    unsigned short   CRC16Bit;
} T_IPC_DATA;
typedef struct T_IPC_MESSAGE_HEADER
{
    uint16_t Id;
    uint16_t Offset;
    uint16_t Size;
    uint16_t Flag;
}T_IPC_HEADER;

typedef struct SpiMsg
{
    uint8_t _type;
}SpiMsg_t;
#define SPI_BUF_SIZE 512
#define FINGERPRINT_ID 2000
typedef struct Spibuf
{
    uint8_t _rbuf[SPI_BUF_SIZE];
    uint8_t _tbuf[SPI_BUF_SIZE];
}Spibuf_t;

void Transfer();
uint8_t Recv_commond();
void Send_commond(uint8_t cmd);
void spiTx_enpacket();
void spiRx_unpacket();
void Spi_init();
void Spibuf_init();
#endif
