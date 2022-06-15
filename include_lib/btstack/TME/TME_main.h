#ifndef __TME_MAIN_H__
#define __TME_MAIN_H__


#include "typedef.h"
#include "btstack/TME/TME_config.h"
#include "btstack/TME/TME_protocol.h"
#include "btstack/TME/TME_msg.h"

#define TME_SPP_SEL   (BIT(0))
#define TME_BLE_SEL   (BIT(1))
#define TME_CON_NULL  (0x00)
#define TME_EDR_MASK  (BIT(7))
void TME_init(u8 connect_select);
void TME_exit(u8 connect_select);

void TME_mic_start();
void TME_mic_stop();

u16  TME_speech_data_send(u8 *buf, u16 len);
u8   get_TME_connect_type(void);
void TME_set_edr_connected(u8 flag);//连接过经典蓝牙
u8   TME_get_edr_connected();//连接过经典蓝牙
#endif
