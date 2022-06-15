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
#include "btcontroller_modules.h"
#include "3th_profile_api.h"
#include "spp_user.h"
#include "btstack/avctp_user.h"
#include "bt_common.h"
#include "classic/tws_api.h"


#if (TCFG_BLE_DEMO_SELECT == DEF_BLE_DEMO_MI)
#include "ble_user.h"
#include "le_common.h"
#include "le_mi_server.h"
#include "custom_cfg.h"
#define TEST_SEND_DATA_RATE          0  //测试 send_data
#define TEST_SEND_HANDLE_VAL         ATT_CHARACTERISTIC_af06_01_VALUE_HANDLE
/* #define TEST_SEND_HANDLE_VAL         ATT_CHARACTERISTIC_ae05_01_VALUE_HANDLE */
#define EXT_ADV_MODE_EN                 0

#define EX_CFG_EN                     0

#if 1
extern void printf_buf(u8 *buf, u32 len);
#define log_info          printf
#define log_info_hexdump  put_buf
#else
#define log_info(...)
#define log_info_hexdump(...)
#endif


/* #define LOG_TAG_CONST       BT_BLE */
/* #define LOG_TAG             "[LE_S_DEMO]" */
/* #define LOG_ERROR_ENABLE */
/* #define LOG_DEBUG_ENABLE */
/* #define LOG_INFO_ENABLE */
/* #define LOG_DUMP_ENABLE */
/* #define LOG_CLI_ENABLE */
/* #include "debug.h" */

//------
#define ATT_LOCAL_PAYLOAD_SIZE    (210)                   //note: need >= 20
#define ATT_SEND_CBUF_SIZE        (4*512)                   //note: need >= 20,缓存大小，可修改
#define ATT_RAM_BUFSIZE           (ATT_CTRL_BLOCK_SIZE + ATT_LOCAL_PAYLOAD_SIZE + ATT_SEND_CBUF_SIZE)                   //note:
static u8 att_ram_buffer[ATT_RAM_BUFSIZE] __attribute__((aligned(4)));
//---------------

#if TEST_SEND_DATA_RATE
static u32 test_data_count;
static u32 server_timer_handle = 0;
static u8 test_data_start;
#endif

//---------------
#define ADV_INTERVAL_DEFAULT        800
static u32 adv_interval_set = ADV_INTERVAL_DEFAULT;

#define HOLD_LATENCY_CNT_MIN  (3)  //(0~0xffff)
#define HOLD_LATENCY_CNT_MAX  (15) //(0~0xffff)
#define HOLD_LATENCY_CNT_ALL  (0xffff)

static volatile hci_con_handle_t con_handle;

//加密设置
static const uint8_t sm_min_key_size = 7;

//连接参数设置
static const uint8_t connection_update_enable = 1; ///0--disable, 1--enable
static uint8_t connection_update_cnt = 0; //
static const struct conn_update_param_t connection_param_table[] = {
    {16, 16, 50,  600},//11
    {12, 28, 30,  600},//3.7
    {8,  20, 30,  600},
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

//用户可配对的，这是样机跟客户开发的app配对的秘钥
const u8 link_key_data[16] = {0x06, 0x77, 0x5f, 0x87, 0x91, 0x8d, 0xd4, 0x23, 0x00, 0x5d, 0xf1, 0xd8, 0xcf, 0x0c, 0x14, 0x2b};
#define EIR_TAG_STRING   0xd6, 0x05, 0x08, 0x00, 'J', 'L', 'A', 'I', 'S', 'D','K'
static const char user_tag_string[] = {EIR_TAG_STRING};

#define adv_data       &att_ram_buffer[0]
#define scan_rsp_data  &att_ram_buffer[32]

static char gap_device_name[BT_NAME_LEN_MAX] = "br22_ble_test";
static u8 gap_device_name_len = 0;
static u8 ble_work_state = 0;
static u8 test_read_write_buf[4];
static u8 adv_ctrl_en;
static u16 app_write_ble_handle = 0;

#define MMA_BLE_ONLY    0x06
#define MMA_DUAL_MODE   0x1a
#define MMA_BT_MODE     MMA_DUAL_MODE

//---------------------------------
//记录手机的虚地址信息
#define BOND_PHONE_INFO_LEN  6 //
static u8 cur_bond_phone_info[BOND_PHONE_INFO_LEN];

enum {
    PHONE_TYPE_DEFAULT = 0,
    PHONE_TYPE_IOS,
    PHONE_TYPE_MAX,
};


static int mi_adv_timeout = 0;
static u8 mi_adv_timeout_init_flag = 0;

#define BLE_CALLBACK_STATE_OFF()  ble_change_addr_state = 1
#define BLE_CALLBACK_STATE_ON()   ble_change_addr_state = 0


extern bool get_last_connect_phone_mac(u8 *mac);
///小米随身2， XM_MAJORID:0x20, MINORID:0x05, VID:0x2717, PID:0x501B
#define COMPANY_ID  (0x038f)
#define XM_VERSION  (0x02)
#define XM_MAJORID  (0x20)     //0x01 mi air 2s
#define XM_MINORID  (0x05)     //0x01 mi air 2s

#define XM_ADV_PARAM_UPDATE_TIMEOUT	(1000)

#pragma pack(1)
typedef union {
    struct {
        u16  company_id;//小米公司ID
        u8  length;
        u8  type;
        u8  major_id;//蓝牙设备的MajorID,由MIUI分配

        u8  tws_l_or_r: 1; //1左耳机 0 右耳机
        u8  minor_id: 2; //蓝牙设备的MinorID,由MIUI分配
        u8  reserved_0: 1;
        u8  l_inquiry_scan: 1;
        u8  r_inquiry_scan: 1; //右耳可被发现状态
        u8  l_page_scan: 1;
        u8  r_page_scan: 1; //右耳可被连接状态

        u8  reserved_1: 1;
        u8  box_status: 1; //【1：开启】，【0：关闭】
        u8  edr_connect_status: 1; //【1：已连接】，【0：未连接】
        u8  edr_identify_status: 1; //【1：已配对】，【0：未配对】
        u8  mac_encryption: 1; //【1：加密】，【0：未加密】。当前未加密
        u8  dev_status: 1; //【1：拿出盒子】，【0：未拿出盒子】
        u8  tws_connect_status: 1; //【1：已连接】，【0：未连接】
        u8  tws_identify_status: 1; //【1：连接中】，【0：未连接中】


        u8 l_battery: 7; //电池信息
        u8 l_charge_status: 1; //左耳充电信息


        u8 r_battery: 7; //
        u8 r_charge_status: 1; //

        u8 box_battery: 7; //
        u8 box_charge_status: 1; //


        u8 phone_lap[3];
        u8 r_mac[6];
        u8 adv_count;
        //1、双MAC设备2个耳机之间要同步
        //2、手机根据计数器变化判断是否弹框
        u8 l_mac[6];
    } _i;
    u8       data[0];
} ADV_HEAD_BIT;

typedef union {
    struct {
        u16 sig;
        u8 length;
        u8 type;
        u8 version;
        u16 pid;
        u8  addr[4];

        u8  reserved_0: 4;
        u8  color: 3;
        u8  reserved_1: 1;
        /* u8  reserved_2[17]; */
    } _i;
    u8 data[0];
} RSP_HEAD_BIT;
#pragma pack()



static volatile u8 charge_box_open_flag = 1;
static volatile u8 mi_server_adv_counter = 5;

//---------------------------------


static void (*app_recieve_callback)(void *priv, void *buf, u16 len) = NULL;
static void (*app_ble_state_callback)(void *priv, ble_state_e state) = NULL;
static void (*ble_resume_send_wakeup)(void) = NULL;
static u32 channel_priv;

static int app_send_user_data_check(u16 len);
static int app_send_user_data_do(void *priv, u8 *data, u16 len);
static int app_send_user_data(u16 handle, u8 *data, u16 len, u8 handle_type);

// Complete Local Name  默认的蓝牙名字

//------------------------------------------------------
//广播参数设置
static void advertisements_setup_init();
extern const char *bt_get_local_name();
static int set_adv_enable(void *priv, u32 en);
static int get_buffer_vaild_len(void *priv);
extern void clr_wdt(void);
//------------------------------------------------------

extern const u8 *bt_get_mac_addr();
void get_edr_address(u8 *addr_buf)
{
    //有个格式要对,need swap
    u8 temp_addr[6];
    memcpy(temp_addr, bt_get_mac_addr(), 6);
    for (int i = 0; i < 6; i++) {
        addr_buf[i] = temp_addr[5 - i];
    }
    printf("edr_addr:");
    put_buf(addr_buf, 6);
}

extern u8 get_vbat_percent(void);
u16 mma_get_bt_battery_val(void)
{
    u8 batter_level = 0;
    batter_level = get_vbat_percent();
    printf("mma batter_level:%d\n", batter_level);
    return batter_level;
}

extern u8 *get_mac_memory_by_index(u8 index);
bool get_last_connect_phone_mac(u8 *mac)
{
    u8 *last_addr = get_mac_memory_by_index(1);
    memcpy(mac, last_addr, 6);
    //memcpy(mac, "123456", 6); //for test
    return true;
}

#define VM_XM_ADV_COUNTER  CFG_BLE_MODE_INFO
static void mi_server_info_save(void)
{
    u8 temp_mma_server_info[10] = {0};
    s32 ret = syscfg_read(VM_XM_ADV_COUNTER, (void *)temp_mma_server_info, 10);
    if (ret == 0) {
        printf("adv counter vm read error  !!\n");
    }
    //printf_buf(temp_mma_server_info, 10);
    mi_server_adv_counter = temp_mma_server_info[0] + 1;;
    temp_mma_server_info[0] = mi_server_adv_counter;
    ret = syscfg_write(VM_XM_ADV_COUNTER, (const void *)temp_mma_server_info, 10);
    if (ret > 0) {
        printf("adv counter vm write !!\n");
    } else {
        printf("adv counter vm write err!!\n");
    }
}




static void __mi_server_mac_enc(u8 *mac_i, u8 *mac_o)
{
    mac_o[0] = mac_i[1];
    mac_o[1] = mac_i[0];
    mac_o[2] = mac_i[2];
    mac_o[3] = mac_i[5];
    mac_o[4] = mac_i[4];
    mac_o[5] = mac_i[3];
}

static int __mi_adv_fill(u8 *buf, int len)
{
    ADV_HEAD_BIT adv_head;
    u8 mac_temp[6];

    if (len < sizeof(ADV_HEAD_BIT)) {
        return 0;
    }


    adv_head._i.company_id          = COMPANY_ID;//小米公司ID
    adv_head._i.length              = 0x16;
    adv_head._i.type                = 0x01;//小米快连协议
    adv_head._i.major_id            = XM_MAJORID;//蓝牙设备的MajorID,由MIUI分配

    adv_head._i.r_page_scan         = 1;//右耳可被连接状态
    adv_head._i.l_page_scan         = 1;
    adv_head._i.r_inquiry_scan      = 1;//右耳可被发现状态
    adv_head._i.l_inquiry_scan      = 1;
    adv_head._i.reserved_0          = 0;
    adv_head._i.minor_id            = XM_MINORID;//蓝牙设备的MinorID,由MIUI分配
    adv_head._i.tws_l_or_r          = 0;//1左耳机 0 右耳机

    adv_head._i.tws_identify_status = 0;//【1：连接中】，【0：未连接中】
    adv_head._i.tws_connect_status  = 1;//【1：已连接】，【0：未连接】//对于该方案， 默认设置为1
    adv_head._i.dev_status          = 0;//【1：拿出盒子】，【0：未拿出盒子】
    adv_head._i.mac_encryption      = 0;//【1：加密】，【0：未加密】。当前未加密
    adv_head._i.edr_identify_status = 0;//【1：已配对】，【0：未配对】
    adv_head._i.edr_connect_status  = ((get_curr_channel_state() != 0) ? 1 : 0);//0;//【1：已连接】，【0：未连接】
    adv_head._i.box_status          = charge_box_open_flag;//【1：开启】，【0：关闭】
    adv_head._i.reserved_1          = 0;

    adv_head._i.l_charge_status     = 0x01;//未知解释 模仿紫米音箱
    adv_head._i.l_battery           = mma_get_bt_battery_val() & 0x7f/*XM_MINORID*/;//0x1;//未知解释 电池信息// 单mac设备， 这里填minorID

    adv_head._i.r_charge_status     = 0x01;//双mac
    adv_head._i.r_battery           = mma_get_bt_battery_val() & 0x7f;

    adv_head._i.box_charge_status   = 0;//1、双MAC设备，标识设备充电盒电池信息
    adv_head._i.box_battery         = 50;//

    u8 phone_mac_addr[6] = {0};
    get_last_connect_phone_mac(phone_mac_addr);
    if (adv_head._i.edr_identify_status == 0) {
        memset(phone_mac_addr, 0x0, 6);
    }
    ///最后连接手机的LAP， mac地址的后三位
    adv_head._i.phone_lap[0]        = phone_mac_addr[0];//0x00;//还需要抓包分析
    adv_head._i.phone_lap[1]        = phone_mac_addr[1];//0x00;
    adv_head._i.phone_lap[2]        = phone_mac_addr[2];//0x00;

    get_edr_address(mac_temp);
    __mi_server_mac_enc(mac_temp, adv_head._i.r_mac);
    adv_head._i.adv_count           = mi_server_adv_counter;//0x3f;//参考紫米音箱
    //1、双MAC设备2个耳机之间要同步
    //2、手机根据计数器变化判断是否弹框
    //
    memset(adv_head._i.l_mac, 0x00, 6);
    memcpy(buf, &adv_head, sizeof(ADV_HEAD_BIT));
    return sizeof(ADV_HEAD_BIT);
}


static int __mi_adv_fill_ex(u8 *buf, int len)
{
    ADV_HEAD_BIT adv_head;
    u8 mac_temp[6];

    if (len < sizeof(ADV_HEAD_BIT)) {
        return 0;
    }


    adv_head._i.company_id          = COMPANY_ID;//小米公司ID
    adv_head._i.length              = 0x16;
    adv_head._i.type                = 0x01;//小米快连协议
    adv_head._i.major_id            = XM_MAJORID;//蓝牙设备的MajorID,由MIUI分配

    adv_head._i.r_page_scan         = 1;//右耳可被连接状态
    adv_head._i.l_page_scan         = 0;
    adv_head._i.r_inquiry_scan      = 1;//右耳可被发现状态
    adv_head._i.l_inquiry_scan      = 0;
    adv_head._i.reserved_0          = 0;
    adv_head._i.minor_id            = 0;//XM_MINORID;//蓝牙设备的MinorID,由MIUI分配
    adv_head._i.tws_l_or_r          = 0;//1左耳机 0 右耳机

    adv_head._i.tws_identify_status = 0;//【1：连接中】，【0：未连接中】
    adv_head._i.tws_connect_status  = 1;//【1：已连接】，【0：未连接】//对于该方案， 默认设置为1
    adv_head._i.dev_status          = 0;//【1：拿出盒子】，【0：未拿出盒子】
    adv_head._i.mac_encryption      = 0;//【1：加密】，【0：未加密】。当前未加密
    adv_head._i.edr_identify_status = 1;//0;//【1：已配对】，【0：未配对】
    adv_head._i.edr_connect_status  = 1;//((get_curr_channel_state() != 0) ? 1 : 0);//0;//【1：已连接】，【0：未连接】
    adv_head._i.box_status          = charge_box_open_flag;//【1：开启】，【0：关闭】//OTA配置默认不打开
    adv_head._i.reserved_1          = 0;

    adv_head._i.l_charge_status     = 0x0;//未知解释 模仿紫米音箱
    adv_head._i.l_battery           = XM_MINORID;//0x1;//未知解释 电池信息// 单mac设备， 这里填minorID

    adv_head._i.r_charge_status     = 0x00;//双mac
    adv_head._i.r_battery           = mma_get_bt_battery_val() & 0x7f;

    adv_head._i.box_charge_status   = 0;//1、双MAC设备，标识设备充电盒电池信息
    adv_head._i.box_battery         = 0;//

    u8 phone_mac_addr[6] = {0};
    get_last_connect_phone_mac(phone_mac_addr);

    ///最后连接手机的LAP， mac地址的后三位
    adv_head._i.phone_lap[0]        = phone_mac_addr[0];//0x00;//还需要抓包分析
    adv_head._i.phone_lap[1]        = phone_mac_addr[1];//0x00;
    adv_head._i.phone_lap[2]        = phone_mac_addr[2];//0x00;

    get_edr_address(mac_temp);
    __mi_server_mac_enc(mac_temp, adv_head._i.r_mac);
    adv_head._i.adv_count           = mi_server_adv_counter;//0x3f;//参考紫米音箱
    //1、双MAC设备2个耳机之间要同步
    //2、手机根据计数器变化判断是否弹框
    //
    memset(adv_head._i.l_mac, 0x00, 6);
    memcpy(buf, &adv_head, sizeof(ADV_HEAD_BIT));
    return sizeof(ADV_HEAD_BIT);
}

extern u16 get_vid_pid_ver_from_cfg_file(u8 type);
u16 mma_get_vid_pid_ver(u8 type)
{
    return get_vid_pid_ver_from_cfg_file(type);
}

extern int get_bt_tws_connect_status();
u8 mma_update_get_tws_status(void)
{
#if (OTA_TWS_SAME_TIME_ENABLE)
    return get_bt_tws_connect_status();
#else
    return 1;
#endif
}

static int __mi_rsp_fill(u8 *buf, int len)
{
    RSP_HEAD_BIT rsp_head ;

    u8 mac_temp[6];

    if (len < sizeof(RSP_HEAD_BIT)) {
        return 0;
    }
    u16 pid = get_vid_pid_ver_from_cfg_file(GET_PID_FROM_EX_CFG);
    u16 vid = get_vid_pid_ver_from_cfg_file(GET_VID_FROM_EX_CFG);


    rsp_head._i.sig           = ((vid >> 8) & 0xff) | ((vid << 8) & 0xff00) ;
    rsp_head._i.length        = 8;
    rsp_head._i.type          = 3;
    rsp_head._i.version       = 2;//版本号2固定
    rsp_head._i.pid           = ((pid >> 8) & 0xff) | ((pid << 8) & 0xff00) ;

    get_edr_address(mac_temp);

    rsp_head._i.addr[0]       = mac_temp[3];
    rsp_head._i.addr[1]       = mac_temp[2];
    rsp_head._i.addr[2]       = mac_temp[1];
    rsp_head._i.addr[3]       = mac_temp[0];

    rsp_head._i.reserved_0    = 0;
    rsp_head._i.color         = 0;
    rsp_head._i.reserved_1    = 0;

    memcpy(buf, &rsp_head, sizeof(RSP_HEAD_BIT));

    return sizeof(RSP_HEAD_BIT);
}


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
        test_data_start = 0;
        return;
    }

    if (test_data_count) {
        log_info("\n%d bytes send: %d.%02d KB/s \n", test_data_count, test_data_count / 1000, test_data_count % 1000);

        test_data_count = 0;
    }
}

static void server_timer_start(void)
{
    if (server_timer_handle) {
        return;
    }

    server_timer_handle  = sys_timer_add(NULL, server_timer_handler, 1000);
}

static void server_timer_stop(void)
{
    if (server_timer_handle) {
        sys_timeout_del(server_timer_handle);
        server_timer_handle = 0;
    }
}


static u8 test_data[256];

void test_data_send_packet(void)
{
    u32 vaild_len = get_buffer_vaild_len(0);
    static u8 test_cnt = 0;

    if (!test_data_start) {
        return;
    }

    if (vaild_len) {

        if (vaild_len > 256) {
            vaild_len = 256;
        }

        test_cnt++;

        memset(test_data, test_cnt, vaild_len);
        /* printf("\n---test_data_len = %d---\n",vaild_len); */
        app_send_user_data(TEST_SEND_HANDLE_VAL, test_data, vaild_len, ATT_OP_AUTO_READ_CCC);
        /* app_send_user_data(ATT_CHARACTERISTIC_ae05_01_VALUE_HANDLE, &test_data_send_packet, vaild_len,ATT_OP_AUTO_READ_CCC); */
        test_data_count += vaild_len;
    }
    clr_wdt();

}
#endif


static void can_send_now_wakeup(void)
{
    /* putchar('E'); */
    if (ble_resume_send_wakeup) {
        ble_resume_send_wakeup();
    }

#if TEST_SEND_DATA_RATE
    test_data_send_packet();
#endif
}

extern void sys_auto_shut_down_disable(void);
extern void sys_auto_shut_down_enable(void);
extern u8 get_total_connect_dev(void);
static void ble_auto_shut_down_enable(u8 enable)
{
#if TCFG_AUTO_SHUT_DOWN_TIME
    if (enable) {
        if (get_total_connect_dev() == 0) {    //已经没有设备连接
            sys_auto_shut_down_enable();
        }
    } else {
        sys_auto_shut_down_disable();
    }
#endif
}

const char *phy_result[] = {
    "None",
    "1M",
    "2M",
    "Coded",
};

static void set_connection_data_length(u16 tx_octets, u16 tx_time)
{
    if (con_handle) {
        ble_user_cmd_prepare(BLE_CMD_SET_DATA_LENGTH, 3, con_handle, tx_octets, tx_time);
    }
}

static void set_connection_data_phy(u8 tx_phy, u8 rx_phy)
{
    if (0 == con_handle) {
        return;
    }

    u8 all_phys = 0;
    u16 phy_options = CONN_SET_PHY_OPTIONS_S8;

    ble_user_cmd_prepare(BLE_CMD_SET_PHY, 5, con_handle, all_phys, tx_phy, rx_phy, phy_options);
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
    u8 status;

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
            case HCI_SUBEVENT_LE_ENHANCED_CONNECTION_COMPLETE:
                status = hci_subevent_le_enhanced_connection_complete_get_status(packet);
                if (status) {
                    log_info("LE_SLAVE CONNECTION FAIL!!! %0x\n", status);
                    set_ble_work_state(BLE_ST_DISCONN);
                    bt_ble_adv_enable(1);
                    break;
                }
                con_handle = hci_subevent_le_enhanced_connection_complete_get_connection_handle(packet);

                ble_user_cmd_prepare(BLE_CMD_ATT_SEND_INIT, 4, con_handle, att_ram_buffer, ATT_RAM_BUFSIZE, ATT_LOCAL_PAYLOAD_SIZE);
                log_info("HCI_SUBEVENT_LE_ENHANCED_CONNECTION_COMPLETE : %0x\n", con_handle);
                log_info("conn_interval = %d\n", hci_subevent_le_enhanced_connection_complete_get_conn_interval(packet));
                log_info("conn_latency = %d\n", hci_subevent_le_enhanced_connection_complete_get_conn_latency(packet));
                log_info("conn_timeout = %d\n", hci_subevent_le_enhanced_connection_complete_get_supervision_timeout(packet));
                break;

            case HCI_SUBEVENT_LE_CONNECTION_COMPLETE: {
                con_handle = hci_subevent_le_connection_complete_get_connection_handle(packet);

                ble_user_cmd_prepare(BLE_CMD_ATT_SEND_INIT, 4, con_handle, att_ram_buffer, ATT_RAM_BUFSIZE, ATT_LOCAL_PAYLOAD_SIZE);
                log_info("HCI_SUBEVENT_LE_CONNECTION_COMPLETE: %0x\n", con_handle);
                connection_update_complete_success(packet + 8);
            }
            break;

            case HCI_SUBEVENT_LE_CONNECTION_UPDATE_COMPLETE:
                connection_update_complete_success(packet);
                break;

            case HCI_SUBEVENT_LE_DATA_LENGTH_CHANGE:
                log_info("APP HCI_SUBEVENT_LE_DATA_LENGTH_CHANGE\n");
                /* set_connection_data_phy(SET_CODED_PHY, SET_CODED_PHY); */
                break;

            case HCI_SUBEVENT_LE_PHY_UPDATE_COMPLETE:
                log_info("APP HCI_SUBEVENT_LE_PHY_UPDATE %s\n", hci_event_le_meta_get_phy_update_complete_status(packet) ? "Fail" : "Succ");
                log_info("Tx PHY: %s\n", phy_result[hci_event_le_meta_get_phy_update_complete_tx_phy(packet)]);
                log_info("Rx PHY: %s\n", phy_result[hci_event_le_meta_get_phy_update_complete_rx_phy(packet)]);
                break;
            }
            break;

        case HCI_EVENT_DISCONNECTION_COMPLETE:
            log_info("HCI_EVENT_DISCONNECTION_COMPLETE: %0x\n", packet[5]);
            con_handle = 0;
            ble_user_cmd_prepare(BLE_CMD_ATT_SEND_INIT, 4, con_handle, 0, 0, 0);
            set_ble_work_state(BLE_ST_DISCONN);
            connection_update_cnt = 0;
            ble_auto_shut_down_enable(1);
            bt_ble_adv_enable(1);
            break;

        case ATT_EVENT_MTU_EXCHANGE_COMPLETE:
            mtu = att_event_mtu_exchange_complete_get_MTU(packet) - 3;
            log_info("ATT MTU = %u\n", mtu);
            ble_user_cmd_prepare(BLE_CMD_ATT_MTU_SIZE, 1, mtu);
            /* set_connection_data_length(251, 2120); */
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

/* LISTING_START(attRead): ATT Read */

// ATT Client Read Callback for Dynamic Data
// - if buffer == NULL, don't copy data, just return size of value
// - if buffer != NULL, copy data and return number bytes copied
// @param offset defines start of attribute value
static uint16_t att_read_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t offset, uint8_t *buffer, uint16_t buffer_size)
{

    uint16_t  att_value_len = 0;
    uint16_t handle = att_handle;

    log_info("read_callback, handle= 0x%04x,buffer= %08x\n", handle, (u32)buffer);

    switch (handle) {
    case ATT_CHARACTERISTIC_2a00_01_VALUE_HANDLE:
        att_value_len = gap_device_name_len;

        if ((offset >= att_value_len) || (offset + buffer_size) > att_value_len) {
            break;
        }

        if (buffer) {
            memcpy(buffer, &gap_device_name[offset], buffer_size);
            att_value_len = buffer_size;
            log_info("\n---xm---read gap_name: %s \n", gap_device_name);
        }
        break;

    case ATT_CHARACTERISTIC_af02_01_CLIENT_CONFIGURATION_HANDLE:
    case ATT_CHARACTERISTIC_af04_01_CLIENT_CONFIGURATION_HANDLE:
    case ATT_CHARACTERISTIC_af06_01_CLIENT_CONFIGURATION_HANDLE:
    case ATT_CHARACTERISTIC_af08_01_CLIENT_CONFIGURATION_HANDLE:
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

static void app_write_revieve_data(u16 handle, u8 *data, u16 len)
{
    log_info("write_hdl %02x -recieve(%d):", handle, len);

    if (handle == ATT_CHARACTERISTIC_af07_01_VALUE_HANDLE
        || (handle == ATT_CHARACTERISTIC_af05_01_VALUE_HANDLE)) {

        printf("$$$-xm_rx(%d):", len);
        put_buf(data, len);
        app_write_ble_handle = handle;
        if (app_recieve_callback) {
            app_recieve_callback((void *)channel_priv, data, len);
        }
    } else {
        //for test
        app_send_user_data(ATT_CHARACTERISTIC_af02_01_VALUE_HANDLE, data, len, ATT_OP_AUTO_READ_CCC);
        app_send_user_data(ATT_CHARACTERISTIC_af04_01_VALUE_HANDLE, data, len, ATT_OP_AUTO_READ_CCC);
    }
}

/* LISTING_START(attWrite): ATT Write */
static int att_write_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size)
{
    int result = 0;
    u16 tmp16;

    u16 handle = att_handle;

    log_info("mi write_callback, handle= 0x%04x,size = %d\n", handle, buffer_size);

    switch (handle) {

    case ATT_CHARACTERISTIC_af05_01_VALUE_HANDLE:
    case ATT_CHARACTERISTIC_af07_01_VALUE_HANDLE:
    case ATT_CHARACTERISTIC_af01_01_VALUE_HANDLE:
    case ATT_CHARACTERISTIC_af03_01_VALUE_HANDLE:
        app_write_revieve_data(handle, buffer, buffer_size);

#if TEST_SEND_DATA_RATE
        if ((buffer[0] == 'A') && (buffer[1] == 'F')) {
            test_data_start = 1;//start
        } else if ((buffer[0] == 'A') && (buffer[1] == 'A')) {
            test_data_start = 0;//stop
        }
#endif

        break;

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

    case ATT_CHARACTERISTIC_af06_01_CLIENT_CONFIGURATION_HANDLE:
    case ATT_CHARACTERISTIC_af08_01_CLIENT_CONFIGURATION_HANDLE:
        log_info("\n------write ccc6:%02x\n", buffer[0]);
        att_set_ccc_config(handle, buffer[0]);
        if (buffer[0]) {
            extern void XM_ble_write_cnonnect_enable(void);
            XM_ble_write_cnonnect_enable();
            set_ble_work_state(BLE_ST_NOTIFY_IDICATE);
            can_send_now_wakeup();
        } else {
            set_ble_work_state(BLE_ST_CONNECT);
        }
        check_connetion_updata_deal();
        break;

    case ATT_CHARACTERISTIC_af02_01_CLIENT_CONFIGURATION_HANDLE:
        log_info("\n------write ccc2:%02x\n", buffer[0]);
        att_set_ccc_config(handle, buffer[0]);
        break;

    case ATT_CHARACTERISTIC_af04_01_CLIENT_CONFIGURATION_HANDLE:
        log_info("\n------write ccc4:%02x\n", buffer[0]);
        att_set_ccc_config(handle, buffer[0]);
        break;

    default:
        break;
    }

    return 0;
}

static int app_send_user_data(u16 handle, u8 *data, u16 len, u8 handle_type)
{
    int ret = APP_BLE_NO_ERROR;

    if (!con_handle) {
        return APP_BLE_OPERATION_ERROR;
    }

    ret = ble_user_cmd_prepare(BLE_CMD_ATT_SEND_DATA, 4, handle, data, len, handle_type);
    if (ret == BLE_BUFFER_FULL) {
        ret = APP_BLE_BUFF_FULL;
    }

    if (ret) {
        os_time_dly(2);
        log_info("ble_send_fail:%d !!!!!! %d\n", ret, len);
    }
    return ret;
}

//------------------------------------------------------
static int make_set_adv_data(void)
{
    u8 offset = 0;
    u8 *buf = adv_data;

    u8 temp[32] = {0};
    offset += make_eir_packet_val(&buf[offset], offset, HCI_EIR_DATATYPE_FLAGS, MMA_BT_MODE, 1);

    int len = __mi_adv_fill(temp, sizeof(temp));
    offset += make_eir_packet_data(&buf[offset], offset, HCI_EIR_DATATYPE_MANUFACTURER_SPECIFIC_DATA, (void *)temp, len);

    if (offset > ADV_RSP_PACKET_MAX) {
        puts("***adv_data overflow!!!!!!\n");
        return -1;
    }
    log_info("adv_data(%d):", offset);
    //log_info_hexdump(buf, offset);
    ble_user_cmd_prepare(BLE_CMD_ADV_DATA, 2, offset, buf);

#if EX_CFG_EN
    offset = 0;
    buf = adv_data_ex;
    memset(temp, 0, 32);
    offset += make_eir_packet_val(&buf[offset], offset, HCI_EIR_DATATYPE_FLAGS, MMA_BT_MODE, 1);
    len = __mi_adv_fill_ex(temp, sizeof(temp));
    offset += make_eir_packet_data(&buf[offset], offset, HCI_EIR_DATATYPE_MANUFACTURER_SPECIFIC_DATA, (void *)temp, len);
    printf("ex adv buf ex = \n");
    put_buf(adv_data_ex, 31);
#endif


    return 0;
}

static int make_set_rsp_data(void)
{
    u8 offset = 0;
    u8 *buf = scan_rsp_data;
    u8 tag_len;
    u8 temp[32] = {0};

    tag_len =  __mi_rsp_fill(temp, sizeof(temp));
    offset += make_eir_packet_data(&buf[offset], offset, HCI_EIR_DATATYPE_MANUFACTURER_SPECIFIC_DATA, (void *)temp, tag_len);

    u8 name_len = gap_device_name_len;
    u8 vaild_len = ADV_RSP_PACKET_MAX - (offset + 2);
    if (name_len > vaild_len) {
        name_len = vaild_len;
    }
    offset += make_eir_packet_data(&buf[offset], offset, HCI_EIR_DATATYPE_COMPLETE_LOCAL_NAME, (void *)gap_device_name, name_len);

    if (offset > ADV_RSP_PACKET_MAX) {
        puts("***rsp_data overflow!!!!!!\n");
        return -1;
    }

    log_info("rsp_data(%d):", offset);
    //log_info_hexdump(buf, offset);
    ble_user_cmd_prepare(BLE_CMD_RSP_DATA, 2, offset, buf);
    return 0;
}

//广播参数设置
static void advertisements_setup_init()
{
    uint8_t adv_type = 0;
    uint8_t adv_channel = 7;
    int   ret = 0;

    ble_user_cmd_prepare(BLE_CMD_ADV_PARAM, 3, adv_interval_set, adv_type, adv_channel);

    ret |= make_set_adv_data();
    ret |= make_set_rsp_data();

    if (ret) {
        puts("advertisements_setup_init fail !!!!!!\n");
        return;
    }

}

#define PASSKEY_ENTER_ENABLE      0 //输入passkey使能，可修改passkey
//重设passkey回调函数，在这里可以重新设置passkey
//passkey为6个数字组成，十万位、万位。。。。个位 各表示一个数字 高位不够为0
static void reset_passkey_cb(u32 *key)
{
#if 1
    u32 newkey = rand32();//获取随机数

    newkey &= 0xfffff;
    if (newkey > 999999) {
        newkey = newkey - 999999; //不能大于999999
    }
    *key = newkey; //小于或等于六位数
    printf("set new_key= %06u\n", *key);
#else
    *key = 123456; //for debug
#endif
}

extern void reset_PK_cb_register(void (*reset_pk)(u32 *));
void ble_sm_setup_init(io_capability_t io_type, u8 auth_req, uint8_t min_key_size, u8 security_en)
{
    //setup SM: Display only
    sm_init();
    sm_set_io_capabilities(io_type);
    sm_set_authentication_requirements(auth_req);
    sm_set_encryption_key_size_range(min_key_size, 16);
    sm_set_request_security(security_en);
    sm_event_callback_set(&cbk_sm_packet_handler);

    if (io_type == IO_CAPABILITY_DISPLAY_ONLY) {
        reset_PK_cb_register(reset_passkey_cb);
    }
}


extern void le_device_db_init(void);
void ble_profile_init(void)
{
    printf("XM mma ble profile init\n");
    le_device_db_init();

#if PASSKEY_ENTER_ENABLE
    ble_sm_setup_init(IO_CAPABILITY_DISPLAY_ONLY, SM_AUTHREQ_MITM_PROTECTION, 7, TCFG_BLE_SECURITY_EN);
#else
    ble_sm_setup_init(IO_CAPABILITY_NO_INPUT_NO_OUTPUT, SM_AUTHREQ_BONDING, 7, TCFG_BLE_SECURITY_EN);
#endif

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

#if EXT_ADV_MODE_EN


#define EXT_ADV_NAME                    'J', 'L', '_', 'E', 'X', 'T', '_', 'A', 'D', 'V'
/* #define EXT_ADV_NAME                    "JL_EXT_ADV" */
#define BYTE_LEN(x...)                  sizeof((u8 []) {x})
#define EXT_ADV_DATA                    \
    0x02, 0x01, 0x06, \
    0x03, 0x02, 0xF0, 0xFF, \
    BYTE_LEN(EXT_ADV_NAME) + 1, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, EXT_ADV_NAME

struct ext_advertising_param {
    u8 Advertising_Handle;
    u16 Advertising_Event_Properties;
    u8 Primary_Advertising_Interval_Min[3];
    u8 Primary_Advertising_Interval_Max[3];
    u8 Primary_Advertising_Channel_Map;
    u8 Own_Address_Type;
    u8 Peer_Address_Type;
    u8 Peer_Address[6];
    u8 Advertising_Filter_Policy;
    u8 Advertising_Tx_Power;
    u8 Primary_Advertising_PHY;
    u8 Secondary_Advertising_Max_Skip;
    u8 Secondary_Advertising_PHY;
    u8 Advertising_SID;
    u8 Scan_Request_Notification_Enable;
} _GNU_PACKED_;

struct ext_advertising_data  {
    u8 Advertising_Handle;
    u8 Operation;
    u8 Fragment_Preference;
    u8 Advertising_Data_Length;
    u8 Advertising_Data[BYTE_LEN(EXT_ADV_DATA)];
} _GNU_PACKED_;

struct ext_advertising_enable {
    u8  Enable;
    u8  Number_of_Sets;
    u8  Advertising_Handle;
    u16 Duration;
    u8  Max_Extended_Advertising_Events;
} _GNU_PACKED_;

const struct ext_advertising_param ext_adv_param = {
    .Advertising_Handle = 0,
    .Advertising_Event_Properties = 1,
    .Primary_Advertising_Interval_Min = {30, 0, 0},
    .Primary_Advertising_Interval_Max = {30, 0, 0},
    .Primary_Advertising_Channel_Map = 7,
    .Primary_Advertising_PHY = 1,
    .Secondary_Advertising_PHY = 1,
};

const struct ext_advertising_data ext_adv_data = {
    .Advertising_Handle = 0,
    .Operation = 3,
    .Fragment_Preference = 0,
    .Advertising_Data_Length = BYTE_LEN(EXT_ADV_DATA),
    .Advertising_Data = EXT_ADV_DATA,
};

const struct ext_advertising_enable ext_adv_enable = {
    .Enable = 1,
    .Number_of_Sets = 1,
    .Advertising_Handle = 0,
    .Duration = 0,
    .Max_Extended_Advertising_Events = 0,
};

const struct ext_advertising_enable ext_adv_disable = {
    .Enable = 0,
    .Number_of_Sets = 1,
    .Advertising_Handle = 0,
    .Duration = 0,
    .Max_Extended_Advertising_Events = 0,
};

#endif /* EXT_ADV_MODE_EN */

static int set_adv_enable(void *priv, u32 en)
{
    ble_state_e next_state, cur_state;

    if (!adv_ctrl_en) {
        return APP_BLE_OPERATION_ERROR;
    }

    if (con_handle) {
        return APP_BLE_OPERATION_ERROR;
    }

    if (en) {
        next_state = BLE_ST_ADV;
    } else {
        next_state = BLE_ST_IDLE;
    }

    cur_state =  get_ble_work_state();
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
        return APP_BLE_NO_ERROR;
    }

    log_info("adv_en:%d\n", en);
    set_ble_work_state(next_state);

    if (en) {
        advertisements_setup_init();
    }
    ble_user_cmd_prepare(BLE_CMD_ADV_ENABLE, 1, en);

    return APP_BLE_NO_ERROR;
}

static int ble_disconnect(void *priv)
{
    if (con_handle) {
        if (BLE_ST_SEND_DISCONN != get_ble_work_state()) {
            log_info(">>>ble send disconnect\n");
            set_ble_work_state(BLE_ST_SEND_DISCONN);
            ble_user_cmd_prepare(BLE_CMD_DISCONNECT, 1, con_handle);
        } else {
            log_info(">>>ble wait disconnect...\n");
        }
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
        log_info("-le_tx(%d):");
        log_info_hexdump(data, len);
    } else {
        putchar('L');
    }
#endif
    if (app_write_ble_handle == ATT_CHARACTERISTIC_af07_01_VALUE_HANDLE) {
        return app_send_user_data(ATT_CHARACTERISTIC_af08_01_VALUE_HANDLE, data, len, ATT_OP_AUTO_READ_CCC);
    } else if (app_write_ble_handle == ATT_CHARACTERISTIC_af05_01_VALUE_HANDLE) {
        return app_send_user_data(ATT_CHARACTERISTIC_af06_01_VALUE_HANDLE, data, len, ATT_OP_AUTO_READ_CCC);
    } else {
        log_info("unkonw get handle:\n");
    }
    return app_send_user_data(ATT_CHARACTERISTIC_af06_01_VALUE_HANDLE, data, len, ATT_OP_AUTO_READ_CCC);
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

u16 bt_ble_is_connected(void)
{
    return con_handle;
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

static const char ble_ext_name[] = "(BLE)";
static void ble_xm_adv(void);

struct ble_server_operation_t *get_mi_ble_operation();
extern void XM_ble_init(struct ble_server_operation_t *ble_handle);
extern void XM_spp_init(struct spp_operation_t *spp_handle);
void bt_ble_init(void)
{
    u8 tmp_ble_addr[6];
    struct spp_operation_t *tmp_spp_handle = NULL;
    log_info("* mi **** ble_init******\n");
    char *name_p;
    u8 ext_name_len = sizeof(ble_ext_name) - 1;

    name_p = bt_get_local_name();
    gap_device_name_len = strlen(name_p);
    if (gap_device_name_len > BT_NAME_LEN_MAX - ext_name_len) {
        gap_device_name_len = BT_NAME_LEN_MAX - ext_name_len;
    }

    //增加后缀，区分名字
    memset(gap_device_name, 0, BT_NAME_LEN_MAX);
    memcpy(gap_device_name, name_p, gap_device_name_len);

    set_ble_work_state(BLE_ST_INIT_OK);
#if (MMA_BT_MODE==MMA_DUAL_MODE)
    //小米APP如果支持双模要同地址才能搜索到显示
    /* bt_set_tx_power(9);//ble txpwer level:0~9 */
    memcpy(tmp_ble_addr, (void *)bt_get_mac_addr(), 6);
#else
    //小米双模采样一样的地址。不需要加后缀区分
    memcpy(&gap_device_name[gap_device_name_len], "(BLE)", ext_name_len);
    gap_device_name_len += ext_name_len;
    lib_make_ble_address(tmp_ble_addr, (void *)bt_get_mac_addr());
#endif //
    log_info("ble name(%d): %s \n", gap_device_name_len, gap_device_name);
    le_controller_set_mac((void *)tmp_ble_addr);
    printf("\n---mi--edr + ble 's address-----");
    printf_buf((void *)bt_get_mac_addr(), 6);
    printf_buf((void *)tmp_ble_addr, 6);
    ble_module_enable(1);

#if TEST_SEND_DATA_RATE
    server_timer_start();
#endif

    extern void XM_protocol_init();
    XM_protocol_init();
    XM_ble_init(get_mi_ble_operation());
    spp_get_operation_table(&tmp_spp_handle);
    XM_spp_init(tmp_spp_handle);
    ble_xm_adv();
}

void bt_ble_exit(void)
{
    log_info("***** ble_exit******\n");


#if TEST_SEND_DATA_RATE
    server_timer_stop();
#endif

    if (mi_adv_timeout) {
        sys_timeout_del(mi_adv_timeout);
        mi_adv_timeout = 0;;
    }
    ble_module_enable(0);
}

void ble_update_address_tws_paired(u8 *comm_addr)
{
    extern void bt_update_mac_addr(u8 * addr);
    u8 tmp_ble_addr[6] = {0};
#if (MMA_BT_MODE==MMA_DUAL_MODE)
    //小米APP如果支持双模要同地址才能搜索到显示
    le_controller_set_mac(comm_addr);
#else
    lib_make_ble_address(tmp_ble_addr, comm_addr);
    le_controller_set_mac((void *)tmp_ble_addr);
#endif //
    bt_update_mac_addr(comm_addr);
    printf("\n---mi tws update --edr + ble 's address-----");
    printf_buf((void *)bt_get_mac_addr(), 6);
    printf_buf((void *)tmp_ble_addr, 6);
}

void ble_app_disconnect(void)
{
    ble_disconnect(NULL);
}

static int set_latency_enable(void *priv, u32 en)
{
    if (!con_handle) {
        return APP_BLE_OPERATION_ERROR;
    }
    log_info("\n-L_EN:%d\n", en);
    return APP_BLE_NO_ERROR;
}

bool ble_get_conn_idf_address(u8 *address)
{
    if (!con_handle) {
        return FALSE;
    }

#if 0
    if (cur_remoter_addr_type == 0) {
        ///public address
        log_info("is public_addr\n");
        memcpy(address, cur_remoter_conn_address, 6);
        return TRUE;
    }

    if (ble_get_peer_addr(cur_remoter_conn_address, address, PAIR_ONLY_ONE_FLAG)) {
        return TRUE;
    }
#endif

    return FALSE;
}


void mi_adv_update_timeout(struct sys_timer *ts)
{
    if (!adv_ctrl_en) {
        return;
    }

    if (get_ble_work_state() == BLE_ST_ADV) {
        bt_ble_adv_enable(0);
        bt_ble_adv_enable(1);
    }
}


void mi_adv_update_timer_init(void)
{
    if (mi_adv_timeout_init_flag == 0) {
        printf("mi_adv_update_timer_init\n");
        mi_adv_timeout_init_flag = 1;
        sys_timer_add(NULL, mi_adv_update_timeout, XM_ADV_PARAM_UPDATE_TIMEOUT);
    }
}


void ble_mi_enable_first_adv(void)
{
    if ((get_ble_work_state() == BLE_ST_INIT_OK)
        || (get_ble_work_state() == BLE_ST_IDLE)
        || (get_ble_work_state() == BLE_ST_DISCONN)) {
        printf("xiaomi first adv enable ,,,,,,,,,,,,,,,\n");
        ble_module_enable(1);
        mi_adv_update_timer_init();
    }
}


ble_state_e XM_ble_cur_state(void)
{
    return  get_ble_work_state();
}

static const struct ble_server_operation_t mi_ble_operation = {
    .adv_enable = set_adv_enable,
    .disconnect = ble_disconnect,
    .get_buffer_vaild = get_buffer_vaild_len,
    .send_data = (void *)app_send_user_data_do,
    .regist_wakeup_send = regiest_wakeup_send,
    .regist_recieve_cbk = regiest_recieve_cbk,
    .regist_state_cbk = regiest_state_cbk,
    .latency_enable = set_latency_enable,
};
struct ble_server_operation_t *get_mi_ble_operation()
{
    return &mi_ble_operation;
}

void ble_xm_adv(void)
{
    log_info("-------------ble_xm_adv-----------------\n");
    //mi_server_update_adv_counter();
    //ble_mi_enable_first_adv();
}

void ble_server_send_test_key_num(u8 key_num)
{
    ;
}

bool ble_msg_deal(u32 param)
{
    struct ble_server_operation_t *test_opt;
    ble_get_server_operation_table(&test_opt);

    return FALSE;
}

void input_key_handler(u8 key_status, u8 key_number)
{
}

extern int tws_api_get_role(void);
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

void test_change_id()
{
    bt_ble_adv_enable(0);
    if (charge_box_open_flag) {
        charge_box_open_flag = 0;
    } else {
        charge_box_open_flag = 1;
        mi_server_adv_counter++;
    }
    mi_server_info_save();
    bt_ble_adv_enable(1);
}



#endif


