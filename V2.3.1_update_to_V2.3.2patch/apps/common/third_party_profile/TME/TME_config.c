#include "app_config.h"

#if (TME_EN)

#include <string.h>
#include <stdlib.h>
#include "btstack/TME/TME_main.h"
#include "syscfg_id.h"
static const char PubKeyB_x[] = {ECHD_KeyX};//{"15207009984421a6586f9fc3fe7e4329d2809ea51125f8ed"};//{"15207009984421a6586f9fc3fe7e4329d2809ea51125f8ed"};
static const char PubKeyB_y[] = {ECHD_KeyY};//{"b09d42b81bc5bd009f79e4b59dbbaa857fca856fb9f7ea25"};//{"b09d42b81bc5bd009f79e4b59dbbaa857fca856fb9f7ea25"};

// tme模式, TME_SUPPORT_MODE_NUM要根据支持的mode数量修改
const u8 tme_support_mode[TME_SUPPORT_MODE_NUM]             = {TME_MODE_INTELLIGENCE_COLLECTION, TME_MODE_GAME_MODE,        TME_MODE_PERSONALIZED_RECOMMENDATION,   TME_MODE_SPORT_MODE};
const u8 tme_factory_mode_position[TME_SUPPORT_MODE_NUM]    = {TME_TRIGGER_POSITION_LEFT,        TME_TRIGGER_POSITION_LEFT, TME_TRIGGER_POSITION_RIGHT,             TME_TRIGGER_POSITION_RIGHT};
const u8 tme_factory_mode_way[TME_SUPPORT_MODE_NUM]         = {TME_TRIGGER_WAY_DOUBLE_CLICK,     TME_TRIGGER_WAY_LONG,      TME_TRIGGER_WAY_THIPLE_CLICK,           TME_TRIGGER_WAY_LONG};

TME_MODE_INFO tme_mode_info[TME_SUPPORT_MODE_NUM];

static u32 tme_pid;// [4] ={0xe8,0x03,0x00,0x00};
static u16 tme_bid;// [2] ={0x01,0x00};
static u16 tme_version;

void tme_mode_info_init()
{
    memset(tme_mode_info, 0, sizeof(tme_mode_info));
    for (int i = 0; i < TME_SUPPORT_MODE_NUM; i++) {
        tme_mode_info[i].mode = tme_support_mode[i];
        tme_mode_info[i].position = tme_factory_mode_position[i];
        tme_mode_info[i].way = tme_factory_mode_way[i];
    }
}

void Get_TME_edr_mac(u8 *data)
{

#if TCFG_USER_TWS_ENABLE
    u8 mac_buf_tmp[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    u8 *mac_buf = data;
    int ret;
    ret = syscfg_read(CFG_TWS_COMMON_ADDR, mac_buf, 6);
    if (ret != 6 || !memcmp(mac_buf, mac_buf_tmp, 6)) {
        do {
            ret = syscfg_read(CFG_BT_MAC_ADDR, mac_buf, 6);
            if ((ret != 6) || !memcmp(mac_buf, mac_buf_tmp, 6)) {
                ASSERT(0, "no mac addr\n");
            }
        } while (0);
    }
#else

    extern const u8 *bt_get_mac_addr();
    const u8 *mac = bt_get_mac_addr();
    data[0] = mac[0];//0x11;
    data[1] = mac[1];//0x12;
    data[2] = mac[2];//0x13;
    data[3] = mac[3];//0x14;
    data[4] = mac[4];//0x15;
    data[5] = mac[5];//0x16;
#endif
    /* printf("\n-----edr + ble 's address-----"); */
    /* put_buf(data,6); */
}

u8 *Get_TME_pid()
{
    //u32 tme_pid;
    tme_pid = atoi(TME_PID);
    return (u8 *)&tme_pid;
}

u8 *Get_TME_bid()
{
    u32 temp = 0;
    temp = atoi(TME_BID);
    tme_bid = temp & 0xffff;
    return (u8 *)&tme_bid;
}


u16 Get_TME_Versions()
{
    tme_version = atoi(PROTOCOL_VERSION);
    return tme_version;
}

u8 Get_TME_dev_battery()
{
    u8 bat = 0;
    extern u8 get_self_battery_level(void);//获取本机电量
    bat = get_self_battery_level() * 10 + 10;
    if (bat > 100) {
        bat = 100;
    }
    return bat;
}

static const char version[] = TME_VERSION;

u16 Get_TME_dev_Versions()
{
    if (sizeof(version) != 2) {
        //printf("%s ,sizeof(version)=%d \n",__func__,sizeof(version));
        return 0;
    }


    return (version[0] << 8) | version[1];
}


const u8 sdp_tme_spp_service_data[96] = {
    0x36, 0x00, 0x5B, 0x09, 0x00, 0x00, 0x0A, 0x00, 0x01, 0x00, 0x04, 0x09, 0x00, 0x01, 0x36, 0x00,
    0x11, 0x1C, 0x00, 0x00, 0xfd, 0x90, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b,
    0x34, 0xfb, 0x09, 0x00, 0x04, 0x36, 0x00, 0x0E, 0x36, 0x00, 0x03, 0x19, 0x01, 0x00, 0x36, 0x00,
    0x05, 0x19, 0x00, 0x03, 0x08, 0x02, 0x09, 0x00, 0x09, 0x36, 0x00, 0x17, 0x36, 0x00, 0x14, 0x1C,
    0x00, 0x00, 0xfd, 0x90, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb,
    0x09, 0x01, 0x00, 0x09, 0x01, 0x00, 0x25, 0x06, 0x4A, 0x4C, 0x5F, 0x53, 0x50, 0x50, 0x00, 0x00,
};

const char *Get_TME_PubKey_x()
{
    return PubKeyB_x;
}

const char *Get_TME_PubKey_y()
{
    return PubKeyB_y;
}

#endif
