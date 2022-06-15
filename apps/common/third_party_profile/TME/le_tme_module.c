/*********************************************************************************************
    *   Filename        : le_server_module.c

    *   Description     :

    *   Author          :

    *   Email           : zh-jieli.com

    *   Last modifiled  : 2017-01-17 11:14

    *   Copyright:(c)JIELI  2011-2016  @ , All Rights Reserved.
*********************************************************************************************/

// *****************************************************************************
/* EXAMPLE_START(le_counter): LE Peripheral - Heartbeat Counter over GATT
 *
 * @text All newer operating systems provide GATT Client functionality.
 * The LE Counter examples demonstrates how to specify a minimal GATT Database
 * with a custom GATT Service and a custom Characteristic that sends periodic
 * notifications.
 */
// *****************************************************************************
#include "system/app_core.h"
#include "system/includes.h"

#include "app_config.h"
#include "app_action.h"

#include "btstack/btstack_task.h"
#include "btstack/bluetooth.h"
#include "user_cfg.h"
#include "vm.h"
#include "app_power_manage.h"
#include "btcontroller_modules.h"
#include "JL_rcsp_api.h"
#include "rcsp_adv_bluetooth.h"
#include "bt_common.h"
#include "bt_tws.h"
#include "3th_profile_api.h"
#include "btstack/avctp_user.h"
#include "app_chargestore.h"
#include "spp_user.h"

#include "le_common.h"
#include "btcrypt.h"
#include "le_tme_module.h"
#include "bt_tws.h"
#include "user_cfg.h"
#include "custom_cfg.h"


#if (TCFG_BLE_DEMO_SELECT == DEF_BLE_TME_ADV)
#define TEST_SEND_DATA_RATE         0  //测试
#define PRINT_DMA_DATA_EN           1


extern void printf_buf(u8 *buf, u32 len);
#if 1
#define log_info          printf
#define log_info_hexdump  printf_buf
#else
#define log_info(...)
#define log_info_hexdump(...)
#endif


#include "btstack/TME/TME_main.h"

void TME_init(u8 connect_select);
void TME_exit(u8 connect_select);


#define EIR_TAG_STRING   0xd6, 0x05, 0x08, 0x00, 'J', 'L', 'A', 'I', 'S', 'D','K'
static const char user_tag_string[] = {EIR_TAG_STRING};
static struct ble_server_operation_t *ble_opt;
#pragma pack(1)
typedef union {
    struct {
        u16 _bid;
        u32 _pid;
        u8  _mac[6];
        u16 _status;
        u8  _cookie[6];
        u16 _versions;
    } _i;
    u8       data[0];
} ADV_HEAD_BIT;
static ADV_HEAD_BIT adv_head;
#pragma pack()

//------
#ifdef ATT_CTRL_BLOCK_SIZE
#undef ATT_CTRL_BLOCK_SIZE
#endif
#define ATT_CTRL_BLOCK_SIZE       (80)                    //note: fixed
#define ATT_LOCAL_PAYLOAD_SIZE    (200)                   //note: need >= 20
#define ATT_SEND_CBUF_SIZE        (512)                   //note: need >= 20,缓存大小，可修改
#define ATT_RAM_BUFSIZE           (ATT_CTRL_BLOCK_SIZE + ATT_LOCAL_PAYLOAD_SIZE + ATT_SEND_CBUF_SIZE)                   //note:
static u8 att_ram_buffer[ATT_RAM_BUFSIZE] __attribute__((aligned(4)));
//---------------

#if TEST_SEND_DATA_RATE
static u32 test_data_count;
static u32 server_timer_handle = 0;
#endif

//---------------
#define ADV_INTERVAL_MIN          (160)

static hci_con_handle_t con_handle;

//加密设置
static const uint8_t sm_min_key_size = 7;

//连接参数设置
static const uint8_t connection_update_enable = 1; ///0--disable, 1--enable
static uint8_t connection_update_cnt = 0; //
static const struct conn_update_param_t connection_param_table[] = {
#if 0
    {8,   8, 0, 600},
    {16, 24, 0, 600},//11
    {12, 28, 0, 600},//3.7
    {8,  20, 0,  600},
#else
    {16, 24, 16,  600},//11
    {12, 28, 14,  600},//3.7
    {8,  20, 20,  600},
#endif
    /* {12, 28, 4, 600},//3.7 */
    /* {12, 24, 30, 600},//3.05 */
};
#define CONN_PARAM_TABLE_CNT      (sizeof(connection_param_table)/sizeof(struct conn_update_param_t))

// 广播包内容
/* static u8 adv_data[ADV_RSP_PACKET_MAX];//max is 31 */
// scan_rsp 内容
/* static u8 scan_rsp_data[ADV_RSP_PACKET_MAX];//max is 31 */

#if (ATT_RAM_BUFSIZE < 64)
#error "adv_data & rsp_data buffer error!!!!!!!!!!!!"
#endif

#define adv_data       &att_ram_buffer[0]
#define scan_rsp_data  &att_ram_buffer[32]

static u32 ble_timer_handle = 0;
static char gap_device_name[BT_NAME_LEN_MAX + 1] = "br22_ble_test";
static u8 gap_device_name_len = 0;
volatile static u8 ble_work_state = 0;
static u8 test_read_write_buf[4];
static u8 adv_ctrl_en;


#define BLE_TIMER_SET (500)

struct _bt_adv_handle {

    u8 seq_rand;
    u8 connect_flag;

    u8 bat_charge_L;
    u8 bat_charge_R;
    u8 bat_charge_C;
    u8 bat_percent_L;
    u8 bat_percent_R;
    u8 bat_percent_C;

    u8 modify_flag;
    u8 adv_disable_cnt;

    u8 ble_adv_notify;
} bt_adv_handle = {0};

#define __this (&bt_adv_handle)

static void (*app_recieve_callback)(void *priv, void *buf, u16 len) = NULL;
static void (*app_ble_state_callback)(void *priv, ble_state_e state) = NULL;
static void (*ble_resume_send_wakeup)(void) = NULL;
static u32 channel_priv;

//------------------------------------------------------

static int app_send_user_data_check(u16 len);
static int app_send_user_data_do(void *priv, u8 *data, u16 len);
static int app_send_user_data(u16 handle, u8 *data, u16 len, u8 handle_type);
//------------------------------------------------------
//广播参数设置
static void advertisements_setup_init();
extern const char *bt_get_local_name();
static int set_adv_enable(void *priv, u32 en);
static int get_buffer_vaild_len(void *priv);
//------------------------------------------------------
static void send_request_connect_parameter(u8 table_index)
{
    struct conn_update_param_t *param = (void *)&connection_param_table[table_index];//static ram

    log_info("update_request:-%d-%d-%d-%d-\n", param->interval_min, param->interval_max, param->latency, param->timeout);
    if (con_handle) {
        ble_user_cmd_prepare(BLE_CMD_REQ_CONN_PARAM_UPDATE, 2, con_handle, param);
    }
}

static void check_connetion_updata_deal(void)
{
    if (connection_update_enable) {
        if (connection_update_cnt < CONN_PARAM_TABLE_CNT) {
            send_request_connect_parameter(connection_update_cnt);
        }
    }
}

static void connection_update_complete_success(u8 *packet)
{
    int con_handle, conn_interval, conn_latency, conn_timeout;

    con_handle = hci_subevent_le_connection_update_complete_get_connection_handle(packet);
    conn_interval = hci_subevent_le_connection_update_complete_get_conn_interval(packet);
    conn_latency = hci_subevent_le_connection_update_complete_get_conn_latency(packet);
    conn_timeout = hci_subevent_le_connection_update_complete_get_supervision_timeout(packet);

    log_info("conn_interval = %d\n", conn_interval);
    log_info("conn_latency = %d\n", conn_latency);
    log_info("conn_timeout = %d\n", conn_timeout);
}


static void set_ble_work_state(ble_state_e state)
{
    if (state != ble_work_state) {
        log_info("ble_work_st:%x->%x\n", ble_work_state, state);
        ble_work_state = state;
        if (app_ble_state_callback) {
            app_ble_state_callback((void *)channel_priv, state);
        }
    }
}

static ble_state_e get_ble_work_state(void)
{
    return ble_work_state;
}

static void cbk_sm_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    sm_just_event_t *event = (void *)packet;
    u32 tmp32;
    switch (packet_type) {
    case HCI_EVENT_PACKET:
        switch (hci_event_packet_get_type(packet)) {
        case SM_EVENT_JUST_WORKS_REQUEST:
            sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
            log_info("Just Works Confirmed.\n");
            break;
        case SM_EVENT_PASSKEY_DISPLAY_NUMBER:
            log_info_hexdump(packet, size);
            memcpy(&tmp32, event->data, 4);
            log_info("Passkey display: %06u.\n", tmp32);
            break;
        }
        break;
    }
}


#if TEST_SEND_DATA_RATE
static void server_timer_handler(void)
{
    if (!con_handle) {
        test_data_count = 0;
        return;
    }

    if (test_data_count) {
        log_info("\n-data_rate: %d bps-\n", test_data_count * 8);
        test_data_count = 0;
    }
}

static void server_timer_start(void)
{
    server_timer_handle  = sys_timer_add(NULL, server_timer_handler, 1000);
}

static void server_timer_stop(void)
{
    if (server_timer_handle) {
        sys_timeout_del(server_timer_handle);
        server_timer_handle = 0;
    }
}

void test_data_send_packet(void)
{
    u32 vaild_len = get_buffer_vaild_len(0);
    if (vaild_len) {
        /* printf("\n---test_data_len = %d---\n",vaild_len); */
        app_send_user_data(ATT_CHARACTERISTIC_ae02_01_VALUE_HANDLE, &test_data_send_packet, vaild_len, ATT_OP_AUTO_READ_CCC);
        /* app_send_user_data(ATT_CHARACTERISTIC_ae05_01_VALUE_HANDLE, &test_data_send_packet, vaild_len,ATT_OP_AUTO_READ_CCC); */
        test_data_count += vaild_len;
    }
}
#endif


static void can_send_now_wakeup(void)
{
    putchar('E');
    if (ble_resume_send_wakeup) {
        ble_resume_send_wakeup();
    }

#if TEST_SEND_DATA_RATE
    test_data_send_packet();
#endif
}

/*
 * @section Packet Handler
 *
 * @text The packet handler is used to:
 *        - stop the counter after a disconnect
 *        - send a notification when the requested ATT_EVENT_CAN_SEND_NOW is received
 */

/* LISTING_START(packetHandler): Packet Handler */
static void cbk_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    int mtu;
    u32 tmp;

    switch (packet_type) {
    case HCI_EVENT_PACKET:
        switch (hci_event_packet_get_type(packet)) {

        /* case DAEMON_EVENT_HCI_PACKET_SENT: */
        /* break; */
        case ATT_EVENT_HANDLE_VALUE_INDICATION_COMPLETE:
            log_info("ATT_EVENT_HANDLE_VALUE_INDICATION_COMPLETE\n");
        case ATT_EVENT_CAN_SEND_NOW:
            can_send_now_wakeup();
            break;

        case HCI_EVENT_LE_META:
            switch (hci_event_le_meta_get_subevent_code(packet)) {
            case HCI_SUBEVENT_LE_CONNECTION_COMPLETE: {
                /* set_app_connect_type(TYPE_BLE); */
                con_handle = little_endian_read_16(packet, 4);
                log_info("HCI_SUBEVENT_LE_CONNECTION_COMPLETE: %0x\n", con_handle);
                connection_update_complete_success(packet + 8);
                ble_user_cmd_prepare(BLE_CMD_ATT_SEND_INIT, 4, con_handle, att_ram_buffer, ATT_RAM_BUFSIZE, ATT_LOCAL_PAYLOAD_SIZE);
                set_ble_work_state(BLE_ST_CONNECT);

                /* rcsp_dev_select(RCSP_BLE); */
                TME_init(TME_BLE_SEL);
                __this->ble_adv_notify = 1;
            }
            break;

            case HCI_SUBEVENT_LE_CONNECTION_UPDATE_COMPLETE:
                connection_update_complete_success(packet);
                break;
            }
            break;

        case HCI_EVENT_DISCONNECTION_COMPLETE:
            log_info("HCI_EVENT_DISCONNECTION_COMPLETE: %0x\n", packet[5]);
            bt_adv_seq_change();
            con_handle = 0;
            /* JL_rcsp_auth_reset(); */
            ble_user_cmd_prepare(BLE_CMD_ATT_SEND_INIT, 4, con_handle, 0, 0, 0);
            set_ble_work_state(BLE_ST_DISCONN);
            /* set_app_connect_type(TYPE_NULL); */
            TME_exit(TME_BLE_SEL);
#if TCFG_USER_TWS_ENABLE
            if (!(tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED)) {
                printf(")))))))) 1\n");
                bt_ble_adv_enable(1);
            } else {
                printf("))))))))>>>> %d\n", tws_api_get_role());
                if (tws_api_get_role() == TWS_ROLE_MASTER) {
                    bt_ble_adv_enable(1);
                }
            }
#else
            bt_ble_adv_enable(1);
#endif
            connection_update_cnt = 0;
            break;

        case ATT_EVENT_MTU_EXCHANGE_COMPLETE:
            mtu = att_event_mtu_exchange_complete_get_MTU(packet) - 3;
            log_info("ATT MTU = %u\n", mtu);
            ble_user_cmd_prepare(BLE_CMD_ATT_MTU_SIZE, 1, mtu);
            break;

        case HCI_EVENT_VENDOR_REMOTE_TEST:
            log_info("--- HCI_EVENT_VENDOR_REMOTE_TEST\n");
            break;

        case L2CAP_EVENT_CONNECTION_PARAMETER_UPDATE_RESPONSE:
            tmp = little_endian_read_16(packet, 4);
            log_info("-update_rsp: %02x\n", tmp);
            if (tmp) {
                connection_update_cnt++;
                log_info("remoter reject!!!\n");
                check_connetion_updata_deal();
            } else {
                connection_update_cnt = CONN_PARAM_TABLE_CNT;
            }
            break;

        case HCI_EVENT_ENCRYPTION_CHANGE:
            log_info("HCI_EVENT_ENCRYPTION_CHANGE= %d\n", packet[2]);
            break;
        }
        break;
    }
}


/* LISTING_END */

/*
 * @section ATT Read
 *
 * @text The ATT Server handles all reads to constant data. For dynamic data like the custom characteristic, the registered
 * att_read_callback is called. To handle long characteristics and long reads, the att_read_callback is first called
 * with buffer == NULL, to request the total value length. Then it will be called again requesting a chunk of the value.
 * See Listing attRead.
 */


static uint16_t att_read_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t offset, uint8_t *buffer, uint16_t buffer_size)
{

    uint16_t  att_value_len = 0;
    uint16_t handle = att_handle;

    /* printf("READ Callback, handle %04x, offset %u, buffer size %u\n", handle, offset, buffer_size); */

    log_info("read_callback, handle= 0x%04x,buffer= %08x\n", handle, (u32)buffer);

    switch (handle) {
    case ATT_CHARACTERISTIC_2a00_01_VALUE_HANDLE:
        att_value_len = strlen(gap_device_name);

        if ((offset >= att_value_len) || (offset + buffer_size) > att_value_len) {
            break;
        }

        if (buffer) {
            memcpy(buffer, &gap_device_name[offset], buffer_size);
            att_value_len = buffer_size;
            log_info("\n------read gap_name: %s \n", gap_device_name);
        }
        break;
    case ATT_CHARACTERISTIC_CA235943_1810_45E6_8326_FC8CA3BC45CE_01_VALUE_HANDLE:
        att_value_len = sizeof(test_read_write_buf);
        if ((offset >= att_value_len) || (offset + buffer_size) > att_value_len) {
            break;
        }

        if (buffer) {
            memcpy(buffer, &test_read_write_buf[offset], buffer_size);
            att_value_len = buffer_size;
        }
        break;
    case ATT_CHARACTERISTIC_CA235943_1810_45E6_8326_FC8CA3BC45CE_01_CLIENT_CONFIGURATION_HANDLE:
        buffer[0] = att_get_ccc_config(handle);
        buffer[1] = 0;
        att_value_len = 2;
        break;

    default:
        break;
    }

    log_info("att_value_len= %d\n", att_value_len);
    return att_value_len;

}


/* LISTING_END */
/*
 * @section ATT Write
 *
 * @text The only valid ATT write in this example is to the Client Characteristic Configuration, which configures notification
 * and indication. If the ATT handle matches the client configuration handle, the new configuration value is stored and used
 * in the heartbeat handler to decide if a new value should be sent. See Listing attWrite.
 */

/* LISTING_START(attWrite): ATT Write */
static int att_write_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size)
{
    int result = 0;
    u16 tmp16;

    u16 handle = att_handle;

    /* log_info("write_callback, handle= 0x%04x\n", handle); */

    switch (handle) {

    case ATT_CHARACTERISTIC_2a00_01_VALUE_HANDLE:
        tmp16 = BT_NAME_LEN_MAX;
        if ((offset >= tmp16) || (offset + buffer_size) > tmp16) {
            break;
        }

        if (offset == 0) {
            memset(gap_device_name, 0x00, BT_NAME_LEN_MAX);
        }
        memcpy(&gap_device_name[offset], buffer, buffer_size);
        log_info("\n------write gap_name:");
        break;

    case ATT_CHARACTERISTIC_CA235943_1810_45E6_8326_FC8CA3BC45CE_01_CLIENT_CONFIGURATION_HANDLE:
        set_ble_work_state(BLE_ST_NOTIFY_IDICATE);
        check_connetion_updata_deal();
        log_info("\n------write ccc:%04x, %02x\n", handle, buffer[0]);
        att_set_ccc_config(handle, buffer[0]);
        can_send_now_wakeup();
        break;

    case ATT_CHARACTERISTIC_68745353_1810_4B13_83A2_C1B21B652C9B_01_VALUE_HANDLE:
        /* printf_buf(buffer,buffer_size); */
        if (app_recieve_callback) {
            app_recieve_callback(0, buffer, buffer_size);
        }
        //		JL_rcsp_auth_recieve(data, len);
        break;

    default:
        break;
    }

    return 0;
}




static int app_send_user_data(u16 handle, u8 *data, u16 len, u8 handle_type)
{
    u32 ret = APP_BLE_NO_ERROR;

    if (!con_handle) {
        return APP_BLE_OPERATION_ERROR;
    }

    ret = ble_user_cmd_prepare(BLE_CMD_ATT_SEND_DATA, 4, handle, data, len, handle_type);
    if (ret == BLE_BUFFER_FULL) {
        ret = APP_BLE_BUFF_FULL;
    }

    if (ret) {
        log_info("app_send_fail:%d !!!!!!\n", ret);
    }
    return ret;
}


//------------------------------------------------------
//------------------------------------------------------
static u8 adv_data_len = 0;
static u8 scan_rcsp_len = 0;
static int make_set_adv_data(void)
{

    u8 offset = 0;
    u8 *buf = adv_data;
    /* extern u8* Get_TME_pid(); */
    /* extern u8* Get_TME_bid(); */
    //printf_buf(Get_TME_bid(),2);
    //printf_buf(Get_TME_pid(),4);
    memcpy(&(adv_head._i._bid), Get_TME_bid(), 2);
    memcpy(&(adv_head._i._pid), Get_TME_pid(), 4);

    Get_TME_edr_mac(&adv_head._i._mac[0]);

    adv_head._i._status =  __this->connect_flag;
    adv_head._i._cookie[0] = 0xcc;
    adv_head._i._cookie[1] = 0xcc;
    adv_head._i._cookie[2] = 0xcc;
    adv_head._i._cookie[3] = 0xcc;
    adv_head._i._cookie[4] = 0xcc;
    adv_head._i._cookie[5] = 0xcc;
    adv_head._i._versions = Get_TME_Versions();

    offset += make_eir_packet_val(&buf[offset], offset, HCI_EIR_DATATYPE_FLAGS, 2, 1);
    //offset += make_eir_packet_val(&buf[offset], offset, HCI_EIR_DATATYPE_COMPLETE_16BIT_SERVICE_UUIDS, 0x1812, 2);

    if (offset > sizeof(adv_data)) {
        puts("***adv_data overflow!!!!!!\n");
        return -1;
    }

    u8 tag_len = sizeof(ADV_HEAD_BIT);
    offset += make_eir_packet_data(&buf[offset], offset, HCI_EIR_DATATYPE_MANUFACTURER_SPECIFIC_DATA, (void *)adv_head.data, tag_len);

    offset += make_eir_packet_val(&buf[offset], offset, HCI_EIR_DATATYPE_COMPLETE_16BIT_SERVICE_UUIDS, 0xfd90, 2);

    printf_buf(adv_data, offset);

    __this->modify_flag = 0;
    ble_user_cmd_prepare(BLE_CMD_ADV_DATA, 2, 31, buf);
    /* ble_printf("ADV data(%d):,offset"); */
    /* ble_put_buf(buf, offset); */
    return 0;
}

static int make_set_rsp_data(void)
{
    u8 offset = 0;
    u8 *buf = scan_rsp_data;
    u8 *edr_name = (u8 *)bt_get_local_name();
    offset += make_eir_packet_val(&buf[offset], offset, HCI_EIR_DATATYPE_TX_POWER_LEVEL, 0xc0, 1);

    u8 name_len = strlen((const char *)edr_name);

    //最后一个减2是测试出来的，那样长度的限制鉴权好像会稳定点。暂无解释----lihaihua
    u8 vaild_len = ADV_RSP_PACKET_MAX - (offset + 2) - 2;

    if (name_len > vaild_len) {
        extern BT_CONFIG bt_cfg;
        extern void lmp_hci_write_local_name(const char *name);
        //蓝牙广播包的名字和经典蓝牙的名字不一样TME的鉴权有问
        //名字太长了，不对应需要把后面的处理掉
        r_printf("make_set_rsp_data===%d--%d\n", LOCAL_NAME_LEN - vaild_len, vaild_len);
        memset(&bt_cfg.edr_name[vaild_len], 0, LOCAL_NAME_LEN - vaild_len);
        lmp_hci_write_local_name(bt_get_local_name());
        name_len = vaild_len;
        gap_device_name_len = name_len;
    }
    //r_printf("name len ===%d\n",name_len);
    offset += make_eir_packet_data(&buf[offset], offset, HCI_EIR_DATATYPE_COMPLETE_LOCAL_NAME, (void *)edr_name, name_len);

    if (offset > ADV_RSP_PACKET_MAX) {
        puts("***rsp_data overflow!!!!!!\n");
        return -1;
    }
    ble_user_cmd_prepare(BLE_CMD_RSP_DATA, 2, 31, buf);
    return 0;
}


u8 *ble_get_scan_rsp_ptr(u8 *len)
{
    u8 offset = 0;
    u8 *buf = scan_rsp_data;

    u8  tag_len = sizeof(user_tag_string);
    offset += make_eir_packet_data(&buf[offset], offset, HCI_EIR_DATATYPE_MANUFACTURER_SPECIFIC_DATA, (void *)user_tag_string, tag_len);
    u8 name_len = gap_device_name_len;
    u8 vaild_len = ADV_RSP_PACKET_MAX - (offset + 2);
    if (name_len > vaild_len) {
        name_len = vaild_len;
    }
    offset += make_eir_packet_data(&buf[offset], offset, HCI_EIR_DATATYPE_COMPLETE_LOCAL_NAME, (void *)gap_device_name, name_len);
    *len = offset;

    return buf;
}

u8 *ble_get_adv_data_ptr(u8 *len)
{
    u8 *buf = adv_data;
    u8 offset = 0;

    offset += make_eir_packet_val(&buf[offset], offset, HCI_EIR_DATATYPE_FLAGS, 6, 1);
    offset += make_eir_packet_val(&buf[offset], offset, HCI_EIR_DATATYPE_COMPLETE_16BIT_SERVICE_UUIDS, 0x1812, 2);
    *len = offset;
    return buf;
}

u8 *ble_get_gatt_profile_data(u16 *len)
{
    *len = sizeof(profile_data);
    return (u8 *)profile_data;
}

//广播参数设置
static void advertisements_setup_init()
{
    uint8_t adv_type = 0;
    uint8_t adv_channel = 7;
    int   ret = 0;
    u16 adv_interval = 0x30;

    ble_user_cmd_prepare(BLE_CMD_ADV_PARAM, 3, adv_interval, adv_type, adv_channel);

    ret |= make_set_adv_data();
    ret |= make_set_rsp_data();

    if (ret) {
        puts("advertisements_setup_init fail !!!!!!\n");
        return;
    }

}


void ble_sm_setup_init(io_capability_t io_type, u8 auth_req, uint8_t min_key_size, u8 security_en)
{
    //setup SM: Display only
    sm_init();
    sm_set_io_capabilities(io_type);
    sm_set_authentication_requirements(auth_req);
    sm_set_encryption_key_size_range(min_key_size, 16);
    sm_set_request_security(security_en);
    sm_event_callback_set(&cbk_sm_packet_handler);
}


extern void le_device_db_init(void);
void ble_profile_init(void)
{
    printf("ble profile init\n");
    le_device_db_init();
    ble_sm_setup_init(IO_CAPABILITY_NO_INPUT_NO_OUTPUT, SM_AUTHREQ_BONDING, 7, TCFG_BLE_SECURITY_EN);
    /* setup ATT server */
    att_server_init(profile_data, att_read_callback, att_write_callback);
    att_server_register_packet_handler(cbk_packet_handler);
    /* gatt_client_register_packet_handler(packet_cbk); */

    // register for HCI events
    hci_event_callback_set(&cbk_packet_handler);
    /* ble_l2cap_register_packet_handler(packet_cbk); */
    /* sm_event_packet_handler_register(packet_cbk); */
    le_l2cap_register_packet_handler(&cbk_packet_handler);
}



static int set_adv_enable(void *priv, u32 en)
{
    ble_state_e next_state, cur_state;

    if (!adv_ctrl_en) {
        return APP_BLE_OPERATION_ERROR;
    }

    printf("con_handle %d\n", con_handle);
    if (con_handle) {
        return APP_BLE_OPERATION_ERROR;
    }

    if (en) {
        next_state = BLE_ST_ADV;
    } else {
        next_state = BLE_ST_IDLE;
    }

    cur_state =  get_ble_work_state();
    printf("get_ble_work_state  %d\n", cur_state);
    switch (cur_state) {
    case BLE_ST_ADV:
    case BLE_ST_IDLE:
    case BLE_ST_INIT_OK:
    case BLE_ST_NULL:
    case BLE_ST_DISCONN:
        break;
    default:
        return APP_BLE_OPERATION_ERROR;
        break;
    }

    if (cur_state == next_state) {
        printf("cur_state == next_state \n");
        return APP_BLE_NO_ERROR;
    }
    log_info("adv_en:%d\n", en);
    printf("adv_en:%d\n", en);
    set_ble_work_state(next_state);
    if (en) {
        advertisements_setup_init();
    }
    ble_user_cmd_prepare(BLE_CMD_ADV_ENABLE, 1, en);
    printf(">  set_adv_enable  %d\n", en);
    return APP_BLE_NO_ERROR;
}


static int ble_disconnect(void *priv)
{
    if (con_handle) {
        set_ble_work_state(BLE_ST_SEND_DISCONN);
        ble_user_cmd_prepare(BLE_CMD_DISCONNECT, 1, con_handle);
        return APP_BLE_NO_ERROR;
    } else {
        return APP_BLE_OPERATION_ERROR;
    }
}

static int get_buffer_vaild_len(void *priv)
{
    u32 vaild_len = 0;
    ble_user_cmd_prepare(BLE_CMD_ATT_VAILD_LEN, 1, &vaild_len);
    return vaild_len;
}

static int app_send_user_data_do(void *priv, u8 *data, u16 len)
{
#if PRINT_DMA_DATA_EN
    if (len < 128) {
        log_info("-tme_tx(%d):", len);
        log_info_hexdump(data, len);
    } else {
        putchar('L');
    }
#endif
    return app_send_user_data(ATT_CHARACTERISTIC_CA235943_1810_45E6_8326_FC8CA3BC45CE_01_VALUE_HANDLE, data, len, ATT_OP_AUTO_READ_CCC);
}

static int app_send_user_data_check(u16 len)
{
    u32 buf_space = get_buffer_vaild_len(0);
    if (len <= buf_space) {
        return 1;
    }
    return 0;
}

static int regiest_wakeup_send(void *priv, void *cbk)
{
    ble_resume_send_wakeup = cbk;
    return APP_BLE_NO_ERROR;
}

static int regiest_recieve_cbk(void *priv, void *cbk)
{
    channel_priv = (u32)priv;
    app_recieve_callback = cbk;
    return APP_BLE_NO_ERROR;
}

static int regiest_state_cbk(void *priv, void *cbk)
{
    channel_priv = (u32)priv;
    app_ble_state_callback = cbk;
    return APP_BLE_NO_ERROR;
}

void bt_ble_adv_enable(u8 enable)
{
    set_adv_enable(0, enable);
}

extern u8 get_charge_online_flag(void);
static u8 update_dev_battery_level(void)
{

    u8 master_bat = 0;
    u8 master_charge = 0;
    u8 slave_bat = 0;
    u8 slave_charge = 0;
    u8 box_bat = 0;
    u8 box_charge = 0;

//	Master bat
#if CONFIG_DISPLAY_DETAIL_BAT
    master_bat = get_vbat_percent();
#else
    master_bat = get_self_battery_level() * 10 + 10;
#endif
    if (master_bat > 100) {
        master_bat = 100;
    }
    master_charge = get_charge_online_flag();


// Slave bat

#if	TCFG_USER_TWS_ENABLE
    slave_bat = get_tws_sibling_bat_persent();

#if TCFG_CHARGESTORE_ENABLE
    if (slave_bat == 0xff) {
        /* log_info("--update_bat_01\n"); */

        slave_bat = chargestore_get_sibling_power_level();
    }
#endif

    if (slave_bat == 0xff) {
        /*  log_info("--update_bat_02\n"); */
        slave_bat = 0x00;
    }

    slave_charge = !!(slave_bat & 0x80);
    slave_bat &= 0x7F;
#else
    slave_charge = 0;
    slave_bat = 0;
#endif

// Box bat
    if ((master_charge && (master_bat != 0))
        || (slave_charge && (slave_bat != 0))) {
        //earphone in charge
#if TCFG_CHARGESTORE_ENABLE
        box_bat = chargestore_get_power_level();
        if (box_bat == 0xFF) {
            box_bat = 0;
        }
#else
        box_bat = 0;
#endif
    } else {
        //all not in charge
        box_bat = 0;
    }
    box_charge = !!(box_bat & 0x80);
    box_bat &= 0x7F;
// check if master is L

    u8 tbat_charge_L = slave_charge;
    u8 tbat_charge_R = master_charge;
    u8 tbat_charge_C = box_charge;
    u8 tbat_percent_L = slave_bat;
    u8 tbat_percent_R = master_bat;
    u8 tbat_percent_C = box_bat;

#if TCFG_CHARGESTORE_ENABLE
    u8 tpercent = 0;
    u8 tcharge = 0;
    if (chargestore_get_earphone_pos() == 'L') {
        //switch position
        log_info("is L pos\n");
        tpercent = tbat_percent_R;
        tcharge = tbat_charge_R;
        tbat_percent_R = tbat_percent_L;
        tbat_charge_R = tbat_charge_L;
        tbat_percent_L = tpercent;
        tbat_charge_L = tcharge;
    } else {
        log_info("is R pos\n");
    }
#endif

// check if change
    u8 tchange = 0;
    if ((tbat_charge_L  != __this->bat_charge_L)
        || (tbat_charge_R  != __this->bat_charge_R)
        || (tbat_charge_C  != __this->bat_charge_C)
        || (tbat_percent_L != __this->bat_percent_L)
        || (tbat_percent_R != __this->bat_percent_R)
        || (tbat_percent_C != __this->bat_percent_C)) {
        tchange = 1;
    }

    __this->bat_charge_L  = tbat_charge_L;
    __this->bat_charge_R  = tbat_charge_R;
    __this->bat_charge_C  = tbat_charge_C;
    __this->bat_percent_L = tbat_percent_L;
    __this->bat_percent_R = tbat_percent_R;
    __this->bat_percent_C = tbat_percent_C;

    /* u8 *buf = adv_data; */
    /* buf[16] = __this->bat_percent_L ? (((!!__this->bat_charge_L) << 7) | (__this->bat_percent_L & 0x7F)):0; */
    /* buf[17] = __this->bat_percent_R ? (((!!__this->bat_charge_R) << 7) | (__this->bat_percent_R & 0x7F)):0; */
    /* buf[18] = __this->bat_percent_C ? (((!!__this->bat_charge_C) << 7) | (__this->bat_percent_C & 0x7F)):0; */

    return tchange;
}



static u8 update_dev_connect_flag(void)
{
    u8 old_flag = __this->connect_flag;
    extern u8 get_bt_connect_status(void);
    if (get_bt_connect_status() ==  BT_STATUS_WAITINT_CONN) {
        __this->connect_flag = !(TME_get_edr_connected());
    } else {
        __this->connect_flag = 0x10;
    }

    if (old_flag != __this->connect_flag) {
        return 1;
    }
    return 0;
}

static void bt_ble_timer_handler(void *para)
{
    u8 *_para = para;

    ble_state_e cur_state =  get_ble_work_state();

    // battery
    if (update_dev_battery_level() || update_dev_connect_flag()) {
        __this->modify_flag = 1;
    }

    if (con_handle && __this->ble_adv_notify) {
        /* printf("adv con_handle %d\n", con_handle); */
        return;
    }

    if (cur_state != BLE_ST_ADV) {
        return;
    }

    if (__this->modify_flag == 0) {
        return;
    }

    printf("adv modify!!!!!! \n");
    bt_ble_adv_enable(0);
    make_set_adv_data();
    make_set_rsp_data();
    bt_ble_adv_enable(1);
}

static void bt_ble_timer_init(void)
{
    ble_timer_handle = sys_timer_add(NULL, bt_ble_timer_handler, BLE_TIMER_SET);
}

void bt_adv_seq_change(void)
{
    u8 trand = 0;

    __this->seq_rand = trand;
}

void bt_ble_init_do(void)
{
    log_info("bt_ble_test_tag_do\n");
    //初始化三个电量值
    /* swapX(bt_get_mac_addr(), &user_tag_data[7], 6); */

    __this->connect_flag = 1;
    __this->bat_charge_L = 1;
    __this->bat_charge_R = 1;
    __this->bat_charge_C = 1;
    __this->bat_percent_L = 100;
    __this->bat_percent_R = 100;
    __this->bat_percent_C = 100;
    __this->modify_flag = 0;
    __this->ble_adv_notify = 1;
    update_dev_battery_level();
    bt_adv_seq_change();
    //update_dev_connect_flag();
}



int bt_ble_adv_ioctl(u32 cmd, u32 priv, u8 mode)
{
    printf(" bt_ble_adv_ioctl %d %d %d\n", cmd, priv, mode);
    ble_state_e cur_state =  get_ble_work_state();
    if (mode) {			// set
        switch (cmd) {
        case BT_ADV_ENABLE:
            break;
        case BT_ADV_DISABLE:
            if (cur_state == BLE_ST_ADV) {
                printf("ADV DISABLE !!!\n");
                bt_ble_adv_enable(0);
                return 0;
            }
            __this->connect_flag = SECNE_DISMISS;
            break;
        case BT_ADV_SET_EDR_CON_FLAG:
            __this->connect_flag = priv;
            if (priv == SECNE_UNCONNECTED) {
                bt_adv_seq_change();
            }
            break;
        case BT_ADV_SET_BAT_CHARGE_L:
            __this->bat_charge_L = priv;
            break;
        case BT_ADV_SET_BAT_CHARGE_R:
            __this->bat_charge_R = priv;
            break;
        case BT_ADV_SET_BAT_CHARGE_C:
            __this->bat_charge_C = priv;
            break;
        case BT_ADV_SET_BAT_PERCENT_L:
            __this->bat_percent_L = priv;
            break;
        case BT_ADV_SET_BAT_PERCENT_R:
            __this->bat_percent_R = priv;
            break;
        case BT_ADV_SET_BAT_PERCENT_C:
            __this->bat_percent_C = priv;
            break;
        case BT_ADV_SET_NOTIFY_EN:
            __this->ble_adv_notify = priv;
            break;
        default:
            return -1;
        }

        __this->modify_flag = 1;
    } else {			// get
        switch (cmd) {
        case BT_ADV_ENABLE:
        case BT_ADV_DISABLE:
            break;
        case BT_ADV_SET_EDR_CON_FLAG:
            break;
        case BT_ADV_SET_BAT_CHARGE_L:
            break;
        case BT_ADV_SET_BAT_CHARGE_R:
            break;
        case BT_ADV_SET_BAT_CHARGE_C:
            break;
        case BT_ADV_SET_BAT_PERCENT_L:
            break;
        case BT_ADV_SET_BAT_PERCENT_R:
            break;
        case BT_ADV_SET_BAT_PERCENT_C:
            break;
        default:
            break;
        }
    }
    return 0;
}

void ble_module_enable(u8 en)
{
    log_info("mode_en:%d\n", en);
    if (en) {
        adv_ctrl_en = 1;
        bt_ble_adv_enable(1);
    } else {
        if (con_handle) {
            adv_ctrl_en = 0;
            ble_disconnect(NULL);
        } else {
            bt_ble_adv_enable(0);
            adv_ctrl_en = 0;
        }
    }
}


void bt_ble_init(void)
{

    log_info("***** ble_init******\n");
    TME_set_edr_connected(0);
    //get edr name
    char *edr_name = (char *)bt_get_local_name();
    gap_device_name_len = strlen(edr_name);
    //check if name space legal
    if (gap_device_name_len > BT_NAME_LEN_MAX) {
        gap_device_name_len = BT_NAME_LEN_MAX;
    }
    //copy edr name to gap_device_name
    memset(gap_device_name, 0x00, sizeof(gap_device_name));
    memcpy(gap_device_name, edr_name, gap_device_name_len);

    log_info("ble name(%d): %s \n", gap_device_name_len, gap_device_name);

    set_ble_work_state(BLE_ST_INIT_OK);
    bt_ble_init_do();
    bt_ble_timer_init();
    ble_module_enable(1);

#if TEST_SEND_DATA_RATE
    server_timer_start();
#endif
}

void bt_ble_exit(void)
{
    log_info("***** ble_exit******\n");
    ble_module_enable(0);

#if TEST_SEND_DATA_RATE
    server_timer_stop();
#endif

}


void ble_app_disconnect(void)
{
    if ((get_ble_work_state() == BLE_ST_CONNECT) || (get_ble_work_state() == BLE_ST_NOTIFY_IDICATE)) {
        ble_disconnect(NULL);
    }
}

static const struct ble_server_operation_t mi_ble_operation = {
    .adv_enable = set_adv_enable,
    .disconnect = ble_disconnect,
    .get_buffer_vaild = get_buffer_vaild_len,
    .send_data = (void *)app_send_user_data_do,
    .regist_wakeup_send = regiest_wakeup_send,
    .regist_recieve_cbk = regiest_recieve_cbk,
    .regist_state_cbk = regiest_state_cbk,
};


void ble_get_server_operation_table(struct ble_server_operation_t **interface_pt)
{
    *interface_pt = (void *)&mi_ble_operation;
}

void ble_server_send_test_key_num(u8 key_num)
{
    if (con_handle) {
        if (get_remote_test_flag()) {
            ble_user_cmd_prepare(BLE_CMD_SEND_TEST_KEY_NUM, 2, con_handle, key_num);
        } else {
            log_info("-not conn testbox\n");
        }
    }
}

bool ble_msg_deal(u32 param)
{
    struct ble_server_operation_t *test_opt;
    ble_get_server_operation_table(&test_opt);

#if 0//for test key
    switch (param) {
    case MSG_BT_PP:
        break;

    case MSG_BT_NEXT_FILE:
        break;

    case MSG_HALF_SECOND:
        /* putchar('H'); */
        break;

    default:
        break;
    }
#endif

    return FALSE;
}

void input_key_handler(u8 key_status, u8 key_number)
{
}

void bt_adv_get_bat(u8 *buf)
{
    buf[0] = __this->bat_percent_L ? (((!!__this->bat_charge_L) << 7) | (__this->bat_percent_L & 0x7F)) : 0;
    buf[1] = __this->bat_percent_R ? (((!!__this->bat_charge_R) << 7) | (__this->bat_percent_R & 0x7F)) : 0;
    buf[2] = __this->bat_percent_C ? (((!!__this->bat_charge_C) << 7) | (__this->bat_percent_C & 0x7F)) : 0;
}


int tme_ble_send_data(u8 *data, u16 len)
{
    if (ble_opt) {
        log_info("tme spp_api_tx(%d) \n", len);
        log_info_hexdump(data, len);
        u32 vaild_len = ble_opt->get_buffer_vaild(0);
        if (vaild_len >= len) {
            return ble_opt->send_data(NULL, data, len);

        } else {
            return SPP_USER_ERR_SEND_BUFF_BUSY;
        }
    }
    return SPP_USER_ERR_SEND_FAIL;
}



static void tme_ble_status_callback(void *priv, ble_state_e status)
{
    switch (status) {
    case BLE_ST_IDLE:
        break;
    case BLE_ST_ADV:
        break;
    case BLE_ST_CONNECT:
        break;
    case BLE_ST_SEND_DISCONN:
        break;
    case BLE_ST_NOTIFY_IDICATE:
        break;
    default:
        break;
    }
}


static void tme_ble_send_wakeup(void)
{
    putchar('W');
    void TME_protocol_resume(void);
    TME_protocol_resume();
}

static void tme_ble_recieve_cbk(void *priv, u8 *buf, u16 len)
{
    log_info("tme --- ble_api_rx(%d) \n", len);
    log_info_hexdump(buf, len);

    //loop send data for test
    /* if (tme_spp_send_data_check(len)) { */
    /* tme_spp_send_data(buf, len); */
    /* } */

#if TCFG_USER_TWS_ENABLE
    if (tws_api_get_role() == TWS_ROLE_MASTER) {
        void TME_protocol_data_recieve(void *priv, void *buf, u16 len);
        TME_protocol_data_recieve(NULL, buf, len);
    }
#else
    void TME_protocol_data_recieve(void *priv, void *buf, u16 len);
    TME_protocol_data_recieve(NULL, buf, len);
#endif
}

void phone_call_close_ble_adv(void)
{
    int role = tws_api_get_role();
    printf("call, role: %d, state: %d", role, ble_work_state);
    if ((role == TWS_ROLE_MASTER) && (BLE_ST_ADV == ble_work_state)) { //BLE正在广播
        //关闭BLE
        bt_ble_adv_enable(0);
    }
}

void phone_call_resume_ble_adv(void)
{
    int role = tws_api_get_role();
    printf("call, role: %d, con_hd: %d\n", role, con_handle);

    if ((get_call_status() == BT_CALL_HANGUP) && (role == TWS_ROLE_MASTER) && (0 == con_handle)) { //BLE未连接，且需要广播
        //开启广播
        bt_ble_adv_enable(1);
    }
}


void tme_ble_init(void)
{
    ble_get_server_operation_table(&ble_opt);
    ble_opt->regist_recieve_cbk(0, tme_ble_recieve_cbk);
    ble_opt->regist_state_cbk(0, tme_ble_status_callback);
    ble_opt->regist_wakeup_send(NULL, tme_ble_send_wakeup);
}



#endif


