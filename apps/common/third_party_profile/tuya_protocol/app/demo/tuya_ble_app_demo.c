#include "tuya_ble_stdlib.h"
#include "tuya_ble_type.h"
#include "tuya_ble_heap.h"
#include "tuya_ble_mem.h"
#include "tuya_ble_api.h"
#include "tuya_ble_port.h"
#include "tuya_ble_main.h"
#include "tuya_ble_secure.h"
#include "tuya_ble_data_handler.h"
#include "tuya_ble_storage.h"
#include "tuya_ble_sdk_version.h"
#include "tuya_ble_utils.h"
#include "tuya_ble_event.h"
#include "tuya_ble_app_demo.h"
//#include "tuya_ble_demo_version.h"
#include "tuya_ble_log.h"
#include "tuya_ota.h"
#include "system/generic/printf.h"

#include "log.h"
#include "system/includes.h"
//#include "ota.h"

tuya_ble_device_param_t device_param = {0};

#define LED_PIN IO_PORTA_01
static bool tuya_led_state = 0;

#define TUYA_INFO_TEST 0

#if TUYA_INFO_TEST
static const char device_id_test[DEVICE_ID_LEN] = "tuya52c534229871";
static const char auth_key_test[AUTH_KEY_LEN] = "gqGPQQl4n540dc6sVPoGoh4fO7a8DzED";
static const uint8_t mac_test[6] = {0xDC, 0x23, 0x4E, 0x3E, 0xBD, 0x3D}; //The actual MAC address is : 66:55:44:33:22:11
#endif /* TUYA_INFO_TEST */

#define APP_CUSTOM_EVENT_1  1
#define APP_CUSTOM_EVENT_2  2
#define APP_CUSTOM_EVENT_3  3
#define APP_CUSTOM_EVENT_4  4
#define APP_CUSTOM_EVENT_5  5

static uint8_t dp_data_array[255 + 3];
static uint16_t dp_data_len = 0;

typedef struct {
    uint8_t data[50];
} custom_data_type_t;

#define TUYA_LEGAL_CHAR(c)       ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))

static u16 tuya_get_one_info(const u8 *in, u8 *out)
{
    int read_len = 0;
    const u8 *p = in;

    while (TUYA_LEGAL_CHAR(*p) && *p != ',') { //read product_uuid
        *out++ = *p++;
        read_len++;
    }
    return read_len;
}

typedef struct __flash_of_lic_para_head {
    s16 crc;
    u16 string_len;
    const u8 para_string[];
} __attribute__((packed)) _flash_of_lic_para_head;

#define LIC_PAGE_OFFSET                80

static bool license_para_head_check(u8 *para)
{
    _flash_of_lic_para_head *head;

    //fill head
    head = (_flash_of_lic_para_head *)para;

    ///crc check
    u8 *crc_data = (u8 *)(para + sizeof(((_flash_of_lic_para_head *)0)->crc));
    u32 crc_len = sizeof(_flash_of_lic_para_head) - sizeof(((_flash_of_lic_para_head *)0)->crc)/*head crc*/ + (head->string_len)/*content crc,include end character '\0'*/;
    s16 crc_sum = 0;

    crc_sum = CRC16(crc_data, crc_len);

    if (crc_sum != head->crc) {
        printf("license crc error !!! %x %x \n", (u32)crc_sum, (u32)head->crc);
        return false;
    }

    return true;
}

const u8 *tuya_get_license_ptr(void)
{
    u32 flash_capacity = sdfile_get_disk_capacity();
    u32 flash_addr = flash_capacity - 256 + LIC_PAGE_OFFSET;
    u8 *lic_ptr = NULL;
    _flash_of_lic_para_head *head;

    printf("flash capacity:%x \n", flash_capacity);
    lic_ptr = (u8 *)sdfile_flash_addr2cpu_addr(flash_addr);

    //head length check
    head = (_flash_of_lic_para_head *)lic_ptr;
    if (head->string_len >= 0xff) {
        printf("license length error !!! \n");
        return NULL;
    }

    ////crc check
    if (license_para_head_check(lic_ptr) == (false)) {
        printf("license head check fail\n");
        return NULL;
    }

    //put_buf(lic_ptr, 128);

    lic_ptr += sizeof(_flash_of_lic_para_head);
    return lic_ptr;
}

static uint8_t read_tuya_product_info_from_flash(uint8_t *read_buf, u16 buflen)
{
    uint8_t *rp = read_buf;
    const uint8_t *tuya_ptr = (uint8_t *)tuya_get_license_ptr();
    //printf("tuya_ptr:");
    //put_buf(tuya_ptr, 69);

    if (tuya_ptr == NULL) {
        return FALSE;
    }
    int data_len = 0;
    data_len = tuya_get_one_info(tuya_ptr, rp);
    //put_buf(rp, data_len);
    if (data_len != 16) {
        printf("read uuid err, data_len:%d", data_len);
        put_buf(rp, data_len);
        return FALSE;
    }
    tuya_ptr += 17;

    rp = read_buf + 16;

    data_len = tuya_get_one_info(tuya_ptr, rp);
    //put_buf(rp, data_len);
    if (data_len != 32) {
        printf("read key err, data_len:%d", data_len);
        put_buf(rp, data_len);
        return FALSE;
    }
    tuya_ptr += 33;

    rp = read_buf + 16 + 32;
    data_len = tuya_get_one_info(tuya_ptr, rp);
    //put_buf(rp, data_len);
    if (data_len != 12) {
        printf("read mac err, data_len:%d", data_len);
        put_buf(rp, data_len);
        return FALSE;
    }
    return TRUE;
}

static u8 ascii_to_hex(u8 in)
{
    if (in >= '0' && in <= '9') {
        return in - '0';
    } else if (in >= 'a' && in <= 'f') {
        return in - 'a' + 0x0a;
    } else if (in >= 'A' && in <= 'F') {
        return in - 'A' + 0x0a;
    } else {
        printf("tuya ascii to hex error, data:0x%x", in);
        return 0;
    }
}

static void parse_mac_data(u8 *in, u8 *out)
{
    for (int i = 0; i < 6; i++) {
        out[i] = (ascii_to_hex(in[2 * i]) << 4) + ascii_to_hex(in[2 * i + 1]);
    }
}


__tuya_info tuya_info;

#define U16_TO_LITTLEENDIAN(x) (((x & 0xff) << 8) + (x & 0xff00))

#define TWO_BYTE_TO_DATA(x) ((x[0] << 8) + x[1])
#define FOUR_BYTE_TO_DATA(x) ((x[0] << 24) + (x[1] << 16) + (x[2] << 8) + x[3])

void custom_data_process(int32_t evt_id, void *data)
{
    custom_data_type_t *event_1_data;
    TUYA_APP_LOG_DEBUG("custom event id = %d", evt_id);
    switch (evt_id) {
    case APP_CUSTOM_EVENT_1:
        event_1_data = (custom_data_type_t *)data;
        TUYA_APP_LOG_HEXDUMP_DEBUG("received APP_CUSTOM_EVENT_1 data:", event_1_data->data, 50);
        break;
    case APP_CUSTOM_EVENT_2:
        break;
    case APP_CUSTOM_EVENT_3:
        break;
    case APP_CUSTOM_EVENT_4:
        break;
    case APP_CUSTOM_EVENT_5:
        break;
    default:
        break;

    }
}

custom_data_type_t custom_data;

void custom_evt_1_send_test(uint8_t data)
{
    tuya_ble_custom_evt_t event;

    for (uint8_t i = 0; i < 50; i++) {
        custom_data.data[i] = data;
    }
    event.evt_id = APP_CUSTOM_EVENT_1;
    event.custom_event_handler = (void *)custom_data_process;
    event.data = &custom_data;
    tuya_ble_custom_event_send(event);
}

static uint16_t sn = 0;
static uint32_t time_stamp = 1587795793;


void tuya_battry_indicate_case()
{
    __battery_indicate_data p_dp_data;

    p_dp_data.id = 2;
    p_dp_data.type = 2;
    p_dp_data.len = U16_TO_LITTLEENDIAN(1);
    p_dp_data.data = tuya_info.battery_info.case_battery;

#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x03)
    tuya_ble_dp_data_report(&p_dp_data, 5);
#endif
#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x04)
    tuya_ble_dp_data_send(sn, DP_SEND_TYPE_ACTIVE, DP_SEND_FOR_CLOUD_PANEL, DP_SEND_WITH_RESPONSE, &p_dp_data, 5);
#endif
}

void tuya_battry_indicate_left()
{
    __battery_indicate_data p_dp_data;

    p_dp_data.id = 3;
    p_dp_data.type = 2;
    p_dp_data.len = U16_TO_LITTLEENDIAN(1);
    p_dp_data.data = tuya_info.battery_info.left_battery;

#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x03)
    tuya_ble_dp_data_report(&p_dp_data, 5);
#endif
#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x04)
    tuya_ble_dp_data_send(sn, DP_SEND_TYPE_ACTIVE, DP_SEND_FOR_CLOUD_PANEL, DP_SEND_WITH_RESPONSE, &p_dp_data, 5);
#endif
}

void tuya_battry_indicate_right()
{
    __battery_indicate_data p_dp_data;

    p_dp_data.id = 4;
    p_dp_data.type = 2;
    p_dp_data.len = U16_TO_LITTLEENDIAN(1);
    p_dp_data.data = tuya_info.battery_info.right_battery;

#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x03)
    tuya_ble_dp_data_report(&p_dp_data, 5);
#endif
#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x04)
    tuya_ble_dp_data_send(sn, DP_SEND_TYPE_ACTIVE, DP_SEND_FOR_CLOUD_PANEL, DP_SEND_WITH_RESPONSE, &p_dp_data, 5);
#endif
}

void tuya_key_info_indicate()
{
    __key_indicate_data p_dp_data;

    uint8_t key_func;
    for (int key_idx = 0; key_idx < 6; key_idx++) {
        p_dp_data.id = key_idx + 19;
        p_dp_data.type = 4;
        p_dp_data.len = U16_TO_LITTLEENDIAN(1);
        switch (key_idx) {
        case 0:
            key_func = tuya_info.key_info.left1;
            break;
        case 1:
            key_func = tuya_info.key_info.right1;
            break;
        case 2:
            key_func = tuya_info.key_info.left2;
            break;
        case 3:
            key_func = tuya_info.key_info.right2;
            break;
        case 4:
            key_func = tuya_info.key_info.left3;
            break;
        case 5:
            key_func = tuya_info.key_info.right3;
            break;
        }
        p_dp_data.data = key_func;

        printf("key reset indicate data:");
        put_buf(&p_dp_data, 5);
#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x03)
        tuya_ble_dp_data_report(&p_dp_data, 5);
#endif
#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x04)
        tuya_ble_dp_data_send(sn, DP_SEND_TYPE_ACTIVE, DP_SEND_FOR_CLOUD_PANEL, DP_SEND_WITH_RESPONSE, &p_dp_data, 5);
#endif
    }
}

void tuya_battery_indicate(u8 left, u8 right, u8 chargebox)
{
    //tuya_led_state_indicate();
    tuya_info.battery_info.left_battery = left;
    tuya_info.battery_info.right_battery = right;
    tuya_info.battery_info.case_battery = chargebox;
    tuya_battry_indicate_right();
    tuya_battry_indicate_left();
    tuya_battry_indicate_case();
}

void tuya_key_reset()
{
    tuya_info.key_info.right1 = 0;
    tuya_info.key_info.right2 = 1;
    tuya_info.key_info.right3 = 2;
    tuya_info.key_info.left1 = 0;
    tuya_info.key_info.left2 = 1;
    tuya_info.key_info.left3 = 2;
    tuya_key_info_indicate();
}
/* 设备音量数据上报 */
void tuya_valume_indicate(u8 valume)
{
    __valume_indicate_data p_dp_data;

    p_dp_data.id = 5;
    p_dp_data.type = 2;
    p_dp_data.len = U16_TO_LITTLEENDIAN(1);
    p_dp_data.data = valume;

#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x03)
    tuya_ble_dp_data_report(&p_dp_data, 5);
#endif
#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x04)
    tuya_ble_dp_data_send(sn, DP_SEND_TYPE_ACTIVE, DP_SEND_FOR_CLOUD_PANEL, DP_SEND_WITH_RESPONSE, &p_dp_data, 5);
#endif
}
/* 音乐播放状态上报 */
void tuya_play_status_indicate(u8 status)
{
    __play_status_indicate_data p_dp_data;

    p_dp_data.id = 7;
    p_dp_data.type = 1;
    p_dp_data.len = U16_TO_LITTLEENDIAN(1);
    p_dp_data.data = status;

#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x03)
    tuya_ble_dp_data_report(&p_dp_data, 5);
#endif
#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x04)
    tuya_ble_dp_data_send(sn, DP_SEND_TYPE_ACTIVE, DP_SEND_FOR_CLOUD_PANEL, DP_SEND_WITH_RESPONSE, &p_dp_data, 5);
#endif
}

#include "tone_player.h"
#include "application/audio_eq.h"
#include "bt_tws.h"
extern u8 key_table_l[KEY_NUM_MAX][KEY_EVENT_MAX];
extern u8 key_table_r[KEY_NUM_MAX][KEY_EVENT_MAX];
static struct TUYA_SYNC_INFO tuya_sync_info;
void set_key_event_by_tuya_info(struct sys_event *event, struct key_event *key, u8 *key_event)
{
    u8 channel = 'U';
    if (get_bt_tws_connect_status()) {
        channel = tws_api_get_local_channel();
        printf("channel:%c\n", channel);
        if ('L' == channel) {
            if ((u32)event->arg == KEY_EVENT_FROM_TWS) {
                *key_event = key_table_r[key->value][key->event];
            } else {
                *key_event = key_table_l[key->value][key->event];
            }
        } else {
            if ((u32)event->arg == KEY_EVENT_FROM_TWS) {
                *key_event = key_table_l[key->value][key->event];
            } else {
                *key_event = key_table_r[key->value][key->event];
            }
        }
    } else {
        printf("key_index:%d, %d\n", key->value, key->event);
        *key_event = key_table_l[key->value][key->event];
        printf("%d\n", *event);
    }
}
/*********************************************************/
/* 涂鸦功能同步到对耳 */
/* tuya_info:对应type的同步数据 */
/* 不同type的info都放到tuya_sync_info进行统一同步 */
/* data_type:对应不同功能 */
/* 索引号对应APP_TWS_TUYA_SYNC_XXX (tuya_ble_app_demo.h) */
/*********************************************************/
static void tuya_sync_info_send(u8 *tuya_info, u8 data_type)
{
#if TCFG_USER_TWS_ENABLE
    printf("666666666666\n");
    tuya_sync_info.key_reset = 0;
    if (get_bt_tws_connect_status()) {
        printf("77777777777 data_type:%d\n", data_type);
        switch (data_type) {
        case APP_TWS_TUYA_SYNC_EQ:
            printf("sync eq info!\n");
            memcpy(tuya_sync_info.eq_info, tuya_info, EQ_SECTION_MAX);
            put_buf(tuya_sync_info.eq_info, EQ_SECTION_MAX);
            break;
        case APP_TWS_TUYA_SYNC_ANC:
            printf("sync anc info!\n");
            break;
        case APP_TWS_TUYA_SYNC_VOLUME:
            tuya_sync_info.volume = *tuya_info;
            printf("sync volume info!\n");
            break;
        case APP_TWS_TUYA_SYNC_KEY_R1:
            tuya_sync_info.key_r1 = *tuya_info;
            tuya_sync_info.key_r2 = key_table_r[0][1];
            tuya_sync_info.key_r3 = key_table_r[0][4];
            tuya_sync_info.key_l1 = key_table_l[0][0];
            tuya_sync_info.key_l2 = key_table_l[0][1];
            tuya_sync_info.key_l3 = key_table_l[0][4];
            printf("sync key_r1 info!\n");
            break;
        case APP_TWS_TUYA_SYNC_KEY_R2:
            tuya_sync_info.key_r1 = key_table_r[0][0];
            tuya_sync_info.key_r2 = *tuya_info;
            tuya_sync_info.key_r3 = key_table_r[0][4];
            tuya_sync_info.key_l1 = key_table_l[0][0];
            tuya_sync_info.key_l2 = key_table_l[0][1];
            tuya_sync_info.key_l3 = key_table_l[0][4];
            printf("sync key_r2 info!\n");
            break;
        case APP_TWS_TUYA_SYNC_KEY_R3:
            tuya_sync_info.key_r1 = key_table_r[0][0];
            tuya_sync_info.key_r2 = key_table_r[0][1];
            tuya_sync_info.key_r3 = *tuya_info;
            tuya_sync_info.key_l1 = key_table_l[0][0];
            tuya_sync_info.key_l2 = key_table_l[0][1];
            tuya_sync_info.key_l3 = key_table_l[0][4];
            printf("sync key_r3 info!\n");
            break;
        case APP_TWS_TUYA_SYNC_KEY_L1:
            tuya_sync_info.key_r1 = key_table_r[0][0];
            tuya_sync_info.key_r2 = key_table_r[0][1];
            tuya_sync_info.key_r3 = key_table_r[0][4];
            tuya_sync_info.key_l1 = *tuya_info;
            tuya_sync_info.key_l2 = key_table_l[0][1];
            tuya_sync_info.key_l3 = key_table_l[0][4];
            printf("sync key_l1 info!\n");
            break;
        case APP_TWS_TUYA_SYNC_KEY_L2:
            tuya_sync_info.key_r1 = key_table_r[0][0];
            tuya_sync_info.key_r2 = key_table_r[0][1];
            tuya_sync_info.key_r3 = key_table_r[0][4];
            tuya_sync_info.key_l1 = key_table_l[0][0];
            tuya_sync_info.key_l2 = *tuya_info;
            tuya_sync_info.key_l3 = key_table_l[0][4];
            printf("sync key_l2 info!\n");
            break;
        case APP_TWS_TUYA_SYNC_KEY_L3:
            tuya_sync_info.key_r1 = key_table_r[0][0];
            tuya_sync_info.key_r2 = key_table_r[0][1];
            tuya_sync_info.key_r3 = key_table_r[0][4];
            tuya_sync_info.key_l1 = key_table_l[0][0];
            tuya_sync_info.key_l2 = key_table_l[0][1];
            tuya_sync_info.key_l3 = *tuya_info;
            printf("sync key_l3 info!\n");
            break;
        case APP_TWS_TUYA_SYNC_FIND_DEVICE:
            tuya_sync_info.find_device = *tuya_info;
            tuya_sync_info.device_conn_flag = 0;
            tuya_sync_info.device_disconn_flag = 0;
            tuya_sync_info.phone_conn_flag = 0;
            tuya_sync_info.phone_disconn_flag = 0;
            printf("sync find_device info!\n");
            break;
        case APP_TWS_TUYA_SYNC_DEVICE_CONN_FLAG:
            tuya_sync_info.find_device = 0;
            tuya_sync_info.device_conn_flag = *tuya_info;
            tuya_sync_info.device_disconn_flag = 0;
            tuya_sync_info.phone_conn_flag = 0;
            tuya_sync_info.phone_disconn_flag = 0;
            printf("sync device_conn_flag info!\n");
            break;
        case APP_TWS_TUYA_SYNC_DEVICE_DISCONN_FLAG:
            tuya_sync_info.find_device = 0;
            tuya_sync_info.device_conn_flag = 0;
            tuya_sync_info.device_disconn_flag = *tuya_info;
            tuya_sync_info.phone_conn_flag = 0;
            tuya_sync_info.phone_disconn_flag = 0;
            printf("sync device_disconn_flag info!\n");
            break;
        case APP_TWS_TUYA_SYNC_PHONE_CONN_FLAG:
            tuya_sync_info.find_device = 0;
            tuya_sync_info.device_conn_flag = 0;
            tuya_sync_info.device_disconn_flag = 0;
            tuya_sync_info.phone_conn_flag = *tuya_info;
            tuya_sync_info.phone_disconn_flag = 0;
            printf("sync phone_conn_flag info!\n");
            break;
        case APP_TWS_TUYA_SYNC_PHONE_DISCONN_FLAG:
            tuya_sync_info.find_device = 0;
            tuya_sync_info.device_conn_flag = 0;
            tuya_sync_info.device_disconn_flag = 0;
            tuya_sync_info.phone_conn_flag = 0;
            tuya_sync_info.phone_disconn_flag = *tuya_info;
            printf("sync phone_disconn_flag info!\n");
            break;
        case APP_TWS_TUYA_SYNC_BT_NAME:
            printf("sync bt name!\n");
            memcpy(tuya_sync_info.bt_name, tuya_info, TUYA_BT_NAME_LEN);
            printf("tuya bt name:%s\n", tuya_sync_info.bt_name);
            break;
        case APP_TWS_TUYA_SYNC_KEY_RESET:
            printf("sync key_reset!\n");
            tuya_sync_info.key_reset = 1;
            break;
        default:
            break;
        }
        /* if (tws_api_get_role() == TWS_ROLE_MASTER) { */
        printf("this is tuya master!\n");
        u8 status = tws_api_send_data_to_sibling(&tuya_sync_info, sizeof(tuya_sync_info), TWS_FUNC_ID_TUYA_STATE);
        printf("status:%d\n", status);
        /* } */
    }
#endif
}
void tuya_eq_data_deal(char *eq_info_data)
{
    char data;
    u8 mode = EQ_MODE_CUSTOM;
    // 自定义修改EQ参数
    for (u8 i = 0; i < EQ_SECTION_MAX; i++) {
        data = eq_info_data[i];
        printf(">>>>> tuya_eq_info:%d", data);
        eq_mode_set_custom_param(i, (s8)data);
    }
    eq_mode_set(mode);
}

static void tuya_eq_data_setting(u8 *eq_setting, u8 tws_sync)
{
    if (!eq_setting) {
        ASSERT(0, "without eq_data!");
    } else {
        if (tws_sync) {
            printf("start eq sync!");
            tuya_sync_info_send(eq_setting, 0);
        }
        tuya_eq_data_deal(eq_setting);
    }
}
void tuya_set_music_volume(int volume)
{
    s16 music_volume;
    music_volume = ((volume + 1) * 16) / 100;
    printf("phone_vol:%d,dac_vol:%d", volume, music_volume);
    app_audio_set_volume(APP_AUDIO_STATE_MUSIC, music_volume, 1);
}
static void find_device()
{
    tone_play_index(IDEX_TONE_NORMAL, 1);
}
#include "key_event_deal.h"
/* 涂鸦app对应功能索引映射到sdk按键枚举 */
u8 tuya_key_event_swith(u8 event)
{
    u8 ret = 0;
    switch (event) {
    case 0:
        ret = KEY_VOL_DOWN;
        break;
    case 1:
        ret = KEY_VOL_UP;
        break;
    case 2:
        ret = KEY_MUSIC_NEXT;
        break;
    case 3:
        ret = KEY_MUSIC_PREV;
        break;
    case 4:
        ret = KEY_MUSIC_PP;
        break;
    case 5:
    case 6:
        ret = KEY_OPEN_SIRI;
        break;
    }
    return ret;
}
#include "user_cfg.h"
void Tuya_change_bt_name(char *name, u8 name_len)
{
    extern BT_CONFIG bt_cfg;
    extern const char *bt_get_local_name();
    extern void lmp_hci_write_local_name(const char *name);
    memset(bt_cfg.edr_name, 0, name_len);
    memcpy(bt_cfg.edr_name, name, name_len);
    printf("name:%s\n", name);
    printf("name:%s\n", bt_cfg.edr_name);
    printf("******************************************\n");
    /* extern void bt_wait_phone_connect_control(u8 enable); */
    /* bt_wait_phone_connect_control(0); */
    lmp_hci_write_local_name(bt_get_local_name());
    /* bt_wait_phone_connect_control(1); */
}
extern struct audio_dac_hdl dac_hdl;
#include "btstack/avctp_user.h"
#include "tone_player.h"
static u16 find_device_timer = 0;
static void tuya_post_key_event(u8 event)
{
    struct sys_event e;
    /* user_send_cmd_prepare(USER_CTRL_ALL_SNIFF_EXIT, 0, NULL); */
    e.type = SYS_KEY_EVENT;
    e.u.key.event = event;
    e.u.key.value = 0;
    e.u.key.init = 1;
    e.arg  = (void *)DEVICE_EVENT_FROM_KEY;
    printf("notify event: %d", event);
    sys_event_notify(&e);
}
void tuya_data_parse(tuya_ble_cb_evt_param_t *event)
{
    uint32_t sn = event->dp_received_data.sn;
    put_buf(event->dp_received_data.p_data, event->dp_received_data.data_len);
    uint16_t buf_len = event->dp_received_data.data_len;

    uint8_t dp_id = event->dp_received_data.p_data[0];
    uint8_t type = event->dp_received_data.p_data[1];
    uint16_t data_len = TWO_BYTE_TO_DATA((&event->dp_received_data.p_data[2]));
    uint8_t *data = &event->dp_received_data.p_data[4];
    printf("\n\n<--------------  tuya_data_parse  -------------->");
    printf("sn = %d, id = %d, type = %d, data_len = %d, data:", sn, dp_id, type, data_len);
    u8 key_value_record[6][6] = {0};
    put_buf(data, data_len);
    u8 value = 0;
    switch (dp_id) {
    case 1:
        //iot播报模式
        printf("tuya iot broadcast set to: %d\n", data[0]);
        break;
    case 5:
        //音量设置  yes
        printf("tuya voice set to :%d\n", data[3]);
        tuya_set_music_volume(data[3]);
        tuya_sync_info_send(&data[3], 2);
        break;
    case 6:
        //切换控制  yes
        printf("tuya change_control: %d\n", data[0]);
        if (a2dp_get_status() != BT_MUSIC_STATUS_STARTING) {
            tuya_post_key_event(KEY_EVENT_CLICK);
        } else {
            if (data[0]) {
                tuya_post_key_event(KEY_EVENT_DOUBLE_CLICK);
            } else {
                tuya_post_key_event(KEY_EVENT_TRIPLE_CLICK);
            }
        }
        break;
    case 7:
        //播放/暂停  yes
        tuya_post_key_event(KEY_EVENT_CLICK);
        printf("tuya play:%d\n", data[0]);
        break;
    case 8:
        //设置降噪模式
#if TCFG_AUDIO_ANC_ENABLE
        anc_mode_switch(data[0], 1);
#endif
        tuya_info.noise_info.noise_mode = data[0];
        printf("tuya noise_mode: %d\n", tuya_info.noise_info.noise_mode);
        break;
    case 9:
        //设置降噪场景
        tuya_info.noise_info.noise_scenes = data[0];
        printf("tuya noise_scenes:%d\n", tuya_info.noise_info.noise_scenes);
        break;
    case 10:
        //设置通透模式
        tuya_info.noise_info.transparency_scenes = data[0];
        printf("tuya transparency_scenes: %d\n", tuya_info.noise_info.transparency_scenes);
        break;
    case 11:
        //设置降噪强度
        tuya_info.noise_info.noise_set = data[3];
        printf("tuya noise_set: %d\n", tuya_info.noise_info.noise_set);
        break;
    case 12:
        //寻找设备
        tuya_sync_info_send(&data[0], 9);
        if (data[0]) {
            find_device_timer = sys_timer_add(NULL, find_device, 1000);
        } else {
            sys_timer_del(find_device_timer);
        }
        printf("tuya find device set:%d\n", data[0]);
        break;
    case 13:
        //设备断连提醒
        tuya_sync_info_send(&data[0], 11);
        if (data[0]) {
            tone_play_index(IDEX_TONE_BT_DISCONN, 1);
        }
        printf("tuya device disconnect notify set:%d", data[0]);
        break;
    case 14:
        //设备重连提醒
        tuya_sync_info_send(&data[0], 10);
        if (data[0]) {
            tone_play_index(IDEX_TONE_BT_CONN, 1);
        }
        printf("tuya device reconnect notify set:%d", data[0]);
        break;
    case 15:
        //手机断连提醒
        tuya_sync_info_send(&data[0], 13);
        if (data[0]) {
            tone_play_index(IDEX_TONE_BT_DISCONN, 1);
        }
        printf("tuya phone disconnect notify set:%d", data[0]);
        break;
    case 16:
        //手机重连提醒
        tuya_sync_info_send(&data[0], 12);
        if (data[0]) {
            tone_play_index(IDEX_TONE_BT_CONN, 1);
        }
        printf("tuya phone reconnect notify set:%d", data[0]);
        break;
    case 17:
        //设置eq模式
        printf("tuya eq_mode set:0x%x", data[0]);
        break;
    case 18:
        // 设置eq参数.此处也会设置eq模式  yes
        if (tuya_info.eq_info.eq_onoff == 0) {
            printf("tuya eq_data set fail, eq_onoff is 0!");
            return;
        }
        tuya_info.eq_info.eq_mode = data[2];
        printf("tuya eq_mode:0x%x\n", tuya_info.eq_info.eq_mode);
        memcpy(tuya_info.eq_info.eq_data, &data[3], EQ_CNT);
        for (int i = 0; i < 10; i++) {
            tuya_info.eq_info.eq_data[i] -= 0xc0;
            printf("tuya eq_data[%d]:%d\n", i, tuya_info.eq_info.eq_data[i]);
        }
        tuya_eq_data_setting(tuya_info.eq_info.eq_data, 1);
        break;
    case 19:
        //左按键1
        key_table_l[0][0] = tuya_key_event_swith(data[0]);
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 6; j++) {
                key_value_record[i][j] = key_table_l[i][j];
                key_value_record[i + 3][j] = key_table_r[i][j];
            }
        }
        value = syscfg_write(TUYA_SYNC_KEY_INFO, key_value_record, sizeof(key_value_record));
        printf("value:%d\n", value);
        tuya_sync_info_send(&key_table_l[0][0], 6);
        printf("key_table_l1:%d, %d\n", data[0], key_table_l[0][0]);
        break;
    case 20:
        //右按键1
        key_table_r[0][0] = tuya_key_event_swith(data[0]);
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 6; j++) {
                key_value_record[i][j] = key_table_l[i][j];
                key_value_record[i + 3][j] = key_table_r[i][j];
            }
        }
        value = syscfg_write(TUYA_SYNC_KEY_INFO, key_value_record, sizeof(key_value_record));
        printf("value:%d\n", value);
        tuya_sync_info_send(&key_table_r[0][0], 3);
        printf("key_table_r1:%d, %d\n", data[0], key_table_r[0][0]);
        break;
    case 21:
        //左按键2
        key_table_l[0][1] = tuya_key_event_swith(data[0]);
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 6; j++) {
                key_value_record[i][j] = key_table_l[i][j];
                key_value_record[i + 3][j] = key_table_r[i][j];
            }
        }
        value = syscfg_write(TUYA_SYNC_KEY_INFO, key_value_record, sizeof(key_value_record));
        printf("value:%d\n", value);
        tuya_sync_info_send(&key_table_l[0][1], 7);
        printf("key_table_l2:%d, %d\n", data[0], key_table_l[0][1]);
        break;
    case 22:
        //右按键2
        key_table_r[0][1] = tuya_key_event_swith(data[0]);
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 6; j++) {
                key_value_record[i][j] = key_table_l[i][j];
                key_value_record[i + 3][j] = key_table_r[i][j];
            }
        }
        value = syscfg_write(TUYA_SYNC_KEY_INFO, key_value_record, sizeof(key_value_record));
        printf("value:%d\n", value);
        tuya_sync_info_send(&key_table_r[0][1], 4);
        printf("key_table_r2:%d, %d\n", data[0], key_table_r[0][1]);
        break;
    case 23:
        //左按键3
        key_table_l[0][4] = tuya_key_event_swith(data[0]);
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 6; j++) {
                key_value_record[i][j] = key_table_l[i][j];
                key_value_record[i + 3][j] = key_table_r[i][j];
            }
        }
        value = syscfg_write(TUYA_SYNC_KEY_INFO, key_value_record, sizeof(key_value_record));
        printf("value:%d\n", value);
        tuya_sync_info_send(&key_table_l[0][4], 8);
        printf("key_table_l3:%d, %d\n", data[0], key_table_l[0][4]);
        break;
    case 24:
        //右按键3
        key_table_r[0][4] = tuya_key_event_swith(data[0]);
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 6; j++) {
                key_value_record[i][j] = key_table_l[i][j];
                key_value_record[i + 3][j] = key_table_r[i][j];
            }
        }
        value = syscfg_write(TUYA_SYNC_KEY_INFO, key_value_record, sizeof(key_value_record));
        printf("value:%d\n", value);
        tuya_sync_info_send(&key_table_r[0][4], 5);
        printf("key_table_r3:%d, %d\n", data[0], key_table_r[0][4]);
        break;
    case 25:
        //按键重置
        printf("tuya key reset");
        key_table_l[0][0] = tuya_key_event_swith(0);
        key_table_l[0][1] = tuya_key_event_swith(1);
        key_table_l[0][4] = tuya_key_event_swith(2);
        key_table_r[0][0] = tuya_key_event_swith(0);
        key_table_r[0][1] = tuya_key_event_swith(1);
        key_table_r[0][4] = tuya_key_event_swith(2);
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 6; j++) {
                key_value_record[i][j] = key_table_l[i][j];
                key_value_record[i + 3][j] = key_table_r[i][j];
            }
        }
        value = syscfg_write(TUYA_SYNC_KEY_INFO, key_value_record, sizeof(key_value_record));
        printf("value:%d\n", value);
        tuya_sync_info_send(NULL, 15);
#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x03)
        tuya_ble_dp_data_report(data, data_len); //1
#endif
#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x04)
        tuya_ble_dp_data_send(sn, DP_SEND_TYPE_ACTIVE, DP_SEND_FOR_CLOUD_PANEL, DP_SEND_WITH_RESPONSE, data, data_len);
#endif
        tuya_key_reset();
        return;
    case 26:
        //入耳检测
        printf("tuya inear_detection set:%d\n", data[0]);
        break;
    case 27:
        //贴合度检测
        printf("tuya fit_detection set:%d\n", data[0]);
        break;
    case 31:
        //倒计时
        u32 count_time = FOUR_BYTE_TO_DATA(data);
        printf("tuya count down:%d\n", count_time);
        break;
    case 43:
        //蓝牙名字
        char *name = malloc(data_len + 1);
        memcpy(name, &data[0], data_len);
        name[data_len] = '\0';
        Tuya_change_bt_name(name, 32);
        syscfg_write(CFG_BT_NAME, name, 32);
        tuya_sync_info_send(name, 14);
        printf("tuya bluetooth name: %s, len:%d\n", name, data_len);
        free(name);
        break;
    case 44:
        // 设置eq开关
        tuya_info.eq_info.eq_onoff = data[0];
        printf("tuya eq_onoff set:%d\n", tuya_info.eq_info.eq_onoff);
        break;
    case 45:
        //设置通透强度
        tuya_info.noise_info.trn_set = data[3];
        printf("tuya trn_set:%d\n", tuya_info.noise_info.trn_set);
        break;
    default:
        printf("unknow control msg len = %d\n, data:", data_len);
        break;
    }
#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x03)
    tuya_ble_dp_data_report(data, data_len); //1
#endif
#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x04)
    tuya_ble_dp_data_send(sn, DP_SEND_TYPE_ACTIVE, DP_SEND_FOR_CLOUD_PANEL, DP_SEND_WITH_RESPONSE, data, data_len);
#endif
    printf("key_l_value:%d, %d, %d\n", key_table_l[0][0], key_table_l[0][1], key_table_l[0][4]);
    printf("key_r_value:%d, %d, %d\n", key_table_r[0][0], key_table_r[0][1], key_table_r[0][4]);
}

static void tuya_cb_handler(tuya_ble_cb_evt_param_t *event)
{
    int16_t result = 0;
    printf("tuya cb_handler event->evt=0x%x\n", event->evt);
    switch (event->evt) {
    case TUYA_BLE_CB_EVT_CONNECTE_STATUS:
        TUYA_APP_LOG_INFO("received tuya ble conncet status update event,current connect status = %d", event->connect_status);
        break;
    case TUYA_BLE_CB_EVT_DP_DATA_RECEIVED:
        printf("liushegnjie..... %s, %d\n", __FUNCTION__, __LINE__);
        tuya_data_parse(event);
        break;
    case TUYA_BLE_CB_EVT_DP_DATA_REPORT_RESPONSE:
        TUYA_APP_LOG_INFO("received dp data report response result code =%d", event->dp_response_data.status);
        break;
    case TUYA_BLE_CB_EVT_DP_DATA_WTTH_TIME_REPORT_RESPONSE:
        TUYA_APP_LOG_INFO("received dp data report response result code =%d", event->dp_response_data.status);
        break;
    case TUYA_BLE_CB_EVT_DP_DATA_WITH_FLAG_REPORT_RESPONSE:
        TUYA_APP_LOG_INFO("received dp data with flag report response sn = %d , flag = %d , result code =%d", event->dp_with_flag_response_data.sn, event->dp_with_flag_response_data.mode
                          , event->dp_with_flag_response_data.status);
        break;
    case TUYA_BLE_CB_EVT_DP_DATA_WITH_FLAG_AND_TIME_REPORT_RESPONSE:
        TUYA_APP_LOG_INFO("received dp data with flag and time report response sn = %d , flag = %d , result code =%d", event->dp_with_flag_and_time_response_data.sn,
                          event->dp_with_flag_and_time_response_data.mode, event->dp_with_flag_and_time_response_data.status);
        break;
    case TUYA_BLE_CB_EVT_UNBOUND:
        TUYA_APP_LOG_INFO("received unbound req");
        break;
    case TUYA_BLE_CB_EVT_ANOMALY_UNBOUND:
        TUYA_APP_LOG_INFO("received anomaly unbound req");
        break;
    case TUYA_BLE_CB_EVT_DEVICE_RESET:
        TUYA_APP_LOG_INFO("received device reset req");
        break;
    case TUYA_BLE_CB_EVT_DP_QUERY:
        TUYA_APP_LOG_INFO("received TUYA_BLE_CB_EVT_DP_QUERY event");
        if (dp_data_len > 0) {
#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x03)
            tuya_ble_dp_data_report(dp_data_array, dp_data_len);
#endif
#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x04)
            tuya_ble_dp_data_send(sn, DP_SEND_TYPE_ACTIVE, DP_SEND_FOR_CLOUD_PANEL, DP_SEND_WITH_RESPONSE, dp_data_array, dp_data_len);
#endif
        }
        break;
    case TUYA_BLE_CB_EVT_OTA_DATA:
        tuya_ota_proc(event->ota_data.type, event->ota_data.p_data, event->ota_data.data_len);
        break;
    case TUYA_BLE_CB_EVT_NETWORK_INFO:
        TUYA_APP_LOG_INFO("received net info : %s", event->network_data.p_data);
        tuya_ble_net_config_response(result);
        break;
    case TUYA_BLE_CB_EVT_WIFI_SSID:
        break;
    case TUYA_BLE_CB_EVT_TIME_STAMP:
        TUYA_APP_LOG_INFO("received unix timestamp : %s ,time_zone : %d", event->timestamp_data.timestamp_string, event->timestamp_data.time_zone);
        break;
    case TUYA_BLE_CB_EVT_TIME_NORMAL:
        break;
    case TUYA_BLE_CB_EVT_DATA_PASSTHROUGH:
        TUYA_APP_LOG_HEXDUMP_DEBUG("received ble passthrough data :", event->ble_passthrough_data.p_data, event->ble_passthrough_data.data_len);
        tuya_ble_data_passthrough(event->ble_passthrough_data.p_data, event->ble_passthrough_data.data_len);
        break;
    default:
        TUYA_APP_LOG_WARNING("app_tuya_cb_queue msg: unknown event type 0x%04x", event->evt);
        break;
    }
    tuya_ble_inter_event_response(event);
}


void tuya_ble_app_init(void)
{
    device_param.device_id_len = 16;    //If use the license stored by the SDK,initialized to 0, Otherwise 16 or 20.
    uint8_t read_buf[16 + 32 + 6 + 1] = {0};

    int ret = 0;
    device_param.use_ext_license_key = 1;
    if (device_param.device_id_len == 16) {
#if TUYA_INFO_TEST
        memcpy(device_param.auth_key, (void *)auth_key_test, AUTH_KEY_LEN);
        memcpy(device_param.device_id, (void *)device_id_test, DEVICE_ID_LEN);
        memcpy(device_param.mac_addr.addr, mac_test, 6);
#else
        ret = read_tuya_product_info_from_flash(read_buf, sizeof(read_buf));
        if (ret == TRUE) {
            uint8_t mac_data[6];
            memcpy(device_param.device_id, read_buf, 16);
            memcpy(device_param.auth_key, read_buf + 16, 32);
            parse_mac_data(read_buf + 16 + 32, mac_data);
            memcpy(device_param.mac_addr.addr, mac_data, 6);
        }
#endif /* TUYA_INFO_TEST */
        device_param.mac_addr.addr_type = TUYA_BLE_ADDRESS_TYPE_RANDOM;
    }
    printf("device_id:");
    put_buf(device_param.device_id, 16);
    printf("auth_key:");
    put_buf(device_param.auth_key, 32);
    printf("mac:");
    put_buf(device_param.mac_addr.addr, 6);

    device_param.p_type = TUYA_BLE_PRODUCT_ID_TYPE_PID;
    device_param.product_id_len = 8;
    memcpy(device_param.product_id, APP_PRODUCT_ID, 8);
    device_param.firmware_version = TY_APP_VER_NUM;
    device_param.hardware_version = TY_HARD_VER_NUM;
    printf("HJY:12345\n");
    tuya_ble_sdk_init(&device_param);
    ret = tuya_ble_callback_queue_register(tuya_cb_handler);
    y_printf("tuya_ble_callback_queue_register,ret=%d\n", ret);

    //tuya_ota_init();

    //TUYA_APP_LOG_INFO("demo project version : "TUYA_BLE_DEMO_VERSION_STR);
    TUYA_APP_LOG_INFO("app version : "TY_APP_VER_STR);

}
