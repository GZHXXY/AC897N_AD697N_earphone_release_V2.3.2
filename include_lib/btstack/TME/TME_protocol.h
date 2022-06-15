#ifndef __TME_PROTOCOL_H__
#define __TME_PROTOCOL_H__

#include "typedef.h"

typedef enum __TME_ERR {
    TME_ERR_NONE = 0x0,
    TME_ERR_SEND_DATA_OVER_LIMIT,
    TME_ERR_SEND_BUSY,
    TME_ERR_SEND_NOT_READY,
    TME_ERR_AUTH,
    TME_ERR_EXIT,
} TME_ERR;


///*< TME_CMD、TME_CMD_response、TME_DATA、TME_DATA_response packet send functions>*/
TME_ERR TME_CMD_send(u8 OpCode, u8 *data, u16 len, u8 request_rsp);
TME_ERR TME_CMD_response_send(u8 OpCode, u8 status, u8 sn, u8 *data, u16 len);
TME_ERR TME_DATA_send(u8 OpCode, u8 *data, u16 len, u8 request_rsp);
TME_ERR TME_DATA_response_send(u8 OpCode, u8 status, u8 sn, u8 *data, u16 len);

///*<TME_CMD、TME_CMD_response、TME_DATA、TME_DATA_response recieve>*/
typedef struct __TME_PRO_CBK {
    /*send function callback, SPP or ble*/
    void *priv;
    bool(*fw_ready)(void *priv);
    s32(*fw_send)(void *priv, void *buf, u16 len);
    /*TME_CMD、TME_CMD_response、TME_DATA、TME_DATA_response packet recieve callback*/
    void (*CMD_resp)(void *priv, u8 OpCode, u8 OpCode_SN, u8 *data, u16 len, u8 err_flag);
    void (*DATA_resp)(void *priv, u8 OpCode_SN, u8 *data, u16 len, u8 err_flag);
    void (*CMD_no_resp)(void *priv, u8 OpCode, u8 *data, u16 len);
    void (*DATA_no_resp)(void *priv, u8 OpCode_SN, u8 *data, u16 len);
    void (*CMD_recieve_resp)(void *priv, u8 OpCode, u8 status, u8 *data, u16 len);
    void (*DATA_recieve_resp)(void *priv, u8 OpCode, u8 status, u8 *data, u16 len);
    u8(*wait_resp_timeout)(void *priv, u8 OpCode, u8 *data, u16 length, u8 counter);
} TME_PRO_CBK;

void TME_protocol_init(const TME_PRO_CBK *cbk, u32 irq_index);
void TME_protocol_exit(void);
void TME_protocol_data_recieve(void *priv, void *buf, u16 len);
void TME_protocol_resume(void);
void TME_protocal_MTU_set(u16 mtu);
u16 TME_protocal_MTU_get(void);


#endif//__TME_PROTOCOL_H__

