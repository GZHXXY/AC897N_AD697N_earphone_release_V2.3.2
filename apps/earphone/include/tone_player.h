#ifndef TONE_PLAYER_H
#define TONE_PLAYER_H

#include "app_config.h"
#include "tone_player_api.h"

#define TONE_NUM_0      		SDFILE_RES_ROOT_PATH"tone/0.*"
#define TONE_NUM_1      		SDFILE_RES_ROOT_PATH"tone/1.*"
#define TONE_NUM_2				SDFILE_RES_ROOT_PATH"tone/2.*"
#define TONE_NUM_3				SDFILE_RES_ROOT_PATH"tone/3.*"
#define TONE_NUM_4				SDFILE_RES_ROOT_PATH"tone/4.*"
#define TONE_NUM_5				SDFILE_RES_ROOT_PATH"tone/5.*"
#define TONE_NUM_6				SDFILE_RES_ROOT_PATH"tone/6.*"
#define TONE_NUM_7				SDFILE_RES_ROOT_PATH"tone/7.*"
#define TONE_NUM_8				SDFILE_RES_ROOT_PATH"tone/8.*"
#define TONE_NUM_9				SDFILE_RES_ROOT_PATH"tone/9.*"
#define TONE_BT_MODE			SDFILE_RES_ROOT_PATH"tone/power_on.*"
#define TONE_BT_CONN       		SDFILE_RES_ROOT_PATH"tone/bt_conn.*"
#define TONE_BT_DISCONN    		SDFILE_RES_ROOT_PATH"tone/bt_dconn.*"
#define TONE_TWS_CONN			SDFILE_RES_ROOT_PATH"tone/tws_conn.*"
#define TONE_TWS_DISCONN		SDFILE_RES_ROOT_PATH"tone/tws_dconn.*"
#define TONE_LOW_POWER			SDFILE_RES_ROOT_PATH"tone/low_power.*"
#define TONE_POWER_OFF			SDFILE_RES_ROOT_PATH"tone/power_off.*"
#define TONE_POWER_ON			SDFILE_RES_ROOT_PATH"tone/power_on.*"
#define TONE_RING				SDFILE_RES_ROOT_PATH"tone/ring.*"
#define TONE_MAX_VOL			SDFILE_RES_ROOT_PATH"tone/vol_max.*"
#define TONE_ANC_OFF			SDFILE_RES_ROOT_PATH"tone/anc_off.*"
#define TONE_ANC_ON				SDFILE_RES_ROOT_PATH"tone/anc_on.*"
#define TONE_TRANSPARENCY		SDFILE_RES_ROOT_PATH"tone/anc_trans.*"
#define TONE_EAR_CHECK			SDFILE_RES_ROOT_PATH"tone/ear_check.*"


#define SINE_WTONE_NORAML           0
#define SINE_WTONE_TWS_CONNECT      1
#define SINE_WTONE_TWS_DISCONNECT   2
#define SINE_WTONE_LOW_POWER        4
#define SINE_WTONE_RING             5
#define SINE_WTONE_MAX_VOLUME       6
#define SINE_WTONE_ADSP				7
#define SINE_WTONE_LOW_LATENRY_IN   8
#define SINE_WTONE_LOW_LATENRY_OUT  9

#if CONFIG_USE_DEFAULT_SINE
#undef TONE_TWS_CONN
#define TONE_TWS_CONN           DEFAULT_SINE_TONE(SINE_WTONE_TWS_CONNECT)
#undef TONE_TWS_DISCONN
#define TONE_TWS_DISCONN        DEFAULT_SINE_TONE(SINE_WTONE_TWS_DISCONNECT)

#undef TONE_MAX_VOL
#define TONE_MAX_VOL            DEFAULT_SINE_TONE(SINE_WTONE_MAX_VOLUME)

#undef TONE_NORMAL
#define TONE_NORMAL            DEFAULT_SINE_TONE(SINE_WTONE_NORAML)
#if BT_PHONE_NUMBER

#else
#undef TONE_RING
#define TONE_RING               DEFAULT_SINE_TONE(SINE_WTONE_RING)
#endif

#undef TONE_LOW_POWER
#define TONE_LOW_POWER          DEFAULT_SINE_TONE(SINE_WTONE_LOW_POWER)
#endif

#define TONE_SIN_NORMAL			DEFAULT_SINE_TONE(SINE_WTONE_NORAML)

#define TONE_LOW_LATENCY_IN     DEFAULT_SINE_TONE(SINE_WTONE_LOW_LATENRY_IN)
#define TONE_LOW_LATENCY_OUT    DEFAULT_SINE_TONE(SINE_WTONE_LOW_LATENRY_OUT)


enum {
    IDEX_TONE_NUM_0,
    IDEX_TONE_NUM_1,
    IDEX_TONE_NUM_2,
    IDEX_TONE_NUM_3,
    IDEX_TONE_NUM_4,
    IDEX_TONE_NUM_5,
    IDEX_TONE_NUM_6,
    IDEX_TONE_NUM_7,
    IDEX_TONE_NUM_8,
    IDEX_TONE_NUM_9,
    IDEX_TONE_BT_MODE,
    IDEX_TONE_BT_CONN,
    IDEX_TONE_BT_DISCONN,
    IDEX_TONE_TWS_CONN,
    IDEX_TONE_TWS_DISCONN,
    IDEX_TONE_LOW_POWER,
    IDEX_TONE_POWER_OFF,
    IDEX_TONE_POWER_ON,
    IDEX_TONE_RING,
    IDEX_TONE_MAX_VOL,
    IDEX_TONE_NORMAL,
    IDEX_TONE_ANC_OFF,
    IDEX_TONE_ANC_ON,
    IDEX_TONE_TRANSPARCNCY,


    IDEX_TONE_NONE = 0xFF,
};



int tone_play_init(void);
int tone_play_index(u8 index, u8 preemption);
int tone_play_index_with_callback(u8 index, u8 preemption, void (*user_evt_handler)(void *priv), void *priv);
int tone_play_index_no_tws(u8 index, u8 preemption);
/*
 *@brief:提示音比较，确认目标提示音和正在播放的提示音是否一致
 *@return: 0 	匹配
 *		   非0	不匹配或者当前没有提示音播放
 *@note:通过提示音索引比较
 */
int tone_name_cmp_by_index(u8 index);

#endif


