#ifndef TUYA_BLE_APP_DEMO_H_
#define TUYA_BLE_APP_DEMO_H_


#ifdef __cplusplus
extern "C" {
#endif



#define APP_PRODUCT_ID          "qkqmchj0"

#define APP_BUILD_FIRMNAME      "tuya_ble_sdk_app_demo_nrf52832"

//固件版本
#define TY_APP_VER_NUM       0x0100
#define TY_APP_VER_STR	     "1.0"

//硬件版本
#define TY_HARD_VER_NUM      0x0100
#define TY_HARD_VER_STR	     "1.0"


/*
typedef enum {
    white,
    colour,
	scene,
	music,
} tuya_light_mode;
*/

#define EQ_CNT 10
#define TUYA_BT_NAME_LEN 20
struct TUYA_SYNC_INFO {
    char eq_info[10];
    u8 bt_name[TUYA_BT_NAME_LEN];
    u8 anc_mode;
    u8 volume;
    u8 key_r1;
    u8 key_r2;
    u8 key_r3;
    u8 key_l1;
    u8 key_l2;
    u8 key_l3;
    u8 find_device;
    u8 device_conn_flag;
    u8 device_disconn_flag;
    u8 phone_conn_flag;
    u8 phone_disconn_flag;
    u8 key_reset;
};
enum {
    APP_TWS_TUYA_SYNC_EQ  = 0,
    APP_TWS_TUYA_SYNC_ANC = 1,
    APP_TWS_TUYA_SYNC_VOLUME = 2,
    APP_TWS_TUYA_SYNC_KEY_R1 = 3,
    APP_TWS_TUYA_SYNC_KEY_R2 = 4,
    APP_TWS_TUYA_SYNC_KEY_R3 = 5,
    APP_TWS_TUYA_SYNC_KEY_L1 = 6,
    APP_TWS_TUYA_SYNC_KEY_L2 = 7,
    APP_TWS_TUYA_SYNC_KEY_L3 = 8,
    APP_TWS_TUYA_SYNC_FIND_DEVICE = 9,
    APP_TWS_TUYA_SYNC_DEVICE_CONN_FLAG = 10,
    APP_TWS_TUYA_SYNC_DEVICE_DISCONN_FLAG = 11,
    APP_TWS_TUYA_SYNC_PHONE_CONN_FLAG = 12,
    APP_TWS_TUYA_SYNC_PHONE_DISCONN_FLAG = 13,
    APP_TWS_TUYA_SYNC_BT_NAME = 14,
    APP_TWS_TUYA_SYNC_KEY_RESET = 15,
};
enum {
    voice_down,
    voice_up,
    next_music,
    prev_music,
    music_play,
    ambient_sound,
    voice_assistant,
};

typedef struct {
    uint8_t eq_onoff;
    uint8_t eq_mode;
    char eq_data[EQ_CNT];
} __eq_info;

typedef struct {
    int trn_set;
    int noise_set;
    uint8_t noise_mode;
    uint8_t noise_scenes;
    uint8_t transparency_scenes;
} __noise_info;

typedef struct {
    uint8_t left1;
    uint8_t left2;
    uint8_t left3;
    uint8_t right1;
    uint8_t right2;
    uint8_t right3;
} __key_info;

typedef struct {
    uint8_t case_battery;
    uint8_t left_battery;
    uint8_t right_battery;
} __battery_info;

typedef struct {
    uint8_t led_state;
    __eq_info eq_info;
    __noise_info noise_info;
    __key_info key_info;
    __battery_info battery_info;
} __tuya_info;

typedef struct {
    uint8_t id;
    uint8_t type;
    uint16_t len;
    uint8_t data;
} __battery_indicate_data;

typedef struct {
    uint8_t id;
    uint8_t type;
    uint16_t len;
    uint8_t data;
} __key_indicate_data;

typedef struct {
    uint8_t id;
    uint8_t type;
    uint16_t len;
    uint8_t data;
} __valume_indicate_data;

typedef struct {
    uint8_t id;
    uint8_t type;
    uint16_t len;
    uint8_t data;
} __play_status_indicate_data;
void tuya_ble_app_init(void);
void tuya_valume_indicate(u8 valume);
void tuya_play_status_indicate(u8 status);

#ifdef __cplusplus
}
#endif

#endif //







