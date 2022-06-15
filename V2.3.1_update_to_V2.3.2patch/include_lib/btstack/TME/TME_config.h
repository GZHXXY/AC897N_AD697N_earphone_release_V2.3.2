#ifndef __TME_CONFIG_H__
#define __TME_CONFIG_H__

#include "typedef.h"

#define   PROTOCOL_VERSION  "0001"//协议版本号 10进制字符

#define   SPP_UUID "0000FD90-0000-1000-8000-00805F9B34FB"//SPP UUID

#define   ECHD_KeyX "15207009984421a6586f9fc3fe7e4329d2809ea51125f8ed" //ECHD_KeyX
#define   ECHD_KeyY "b09d42b81bc5bd009f79e4b59dbbaa857fca856fb9f7ea25" //ECHD_Keyy

#define   TME_BID       "0001"//厂商id  10进制字符
#define   TME_PID       "1000"//设备id  10进制字符

#define   TME_VERSION   {0x00,0x10}//版本号

#define TME_SUPPORT_MODE_NUM    4



typedef struct  __TME_MODE_INFO {
    uint8_t mode;
    uint8_t reserved_word;
    uint8_t position;
    uint8_t way;
    uint8_t mode_state;
} TME_MODE_INFO;

extern u8 *Get_TME_pid();
extern u8 *Get_TME_bid();
extern void Get_TME_edr_mac(u8 *data);
extern u16 Get_TME_Versions();
extern u16 Get_TME_dev_Versions();
extern const char *Get_TME_PubKey_x();
extern const char *Get_TME_PubKey_y();
extern const u8    sdp_tme_spp_service_data[];
extern u8 Get_TME_dev_battery();

#endif

