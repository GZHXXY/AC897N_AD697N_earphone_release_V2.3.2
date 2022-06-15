#include "app_config.h"

#include "earphone.h"
#include "app_main.h"
#include "update_tws.h"
#include "3th_profile_api.h"

#include "btstack/avctp_user.h"
#include "btstack/btstack_task.h"
#include "bt_tws.h"
#include "spp_user.h"

#if TME_EN
extern int  bt_ble_adv_ioctl(u32 cmd, u32 priv, u8 mode);
extern void bt_adv_seq_change(void);
extern void tme_ble_init(void);
extern void tme_spp_init(void);
extern void bt_ble_adv_enable(u8 enable);
extern void ble_app_disconnect(void);
extern void ble_app_disconnect(void);
extern void TME_set_edr_connected(u8 flag);
bool JL_spp_fw_ready(void *priv);
void tme_spp_disconnect(void *priv);


#if OTA_TWS_SAME_TIME_NEW       //使用新的同步升级流程
#include "update_tws_new.h"
#else
#include "update_tws.h"
#endif

#define LOG_TAG             "[TRANS_DATA]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"


int tme_earphone_state_set_page_scan_enable()
{
    return 0;
}

int tme_earphone_state_get_connect_mac_addr()
{
    return 0;
}

int tme_earphone_state_cancel_page_scan()
{
    return 0;
}

int tme_earphone_state_tws_init(int paired)
{
    if (paired) {
        printf(">>>>>>>>>tme_earphone_state_tws_init disable auto role switch \n");
        tws_api_auto_role_switch_disable();
    }
    return 0;
}

int tme_earphone_state_tws_connected(int first_pair, u8 *comm_addr)
{
    if (first_pair) {
        extern void ble_module_enable(u8 en);
        extern void bt_update_mac_addr(u8 * addr);
        extern void lib_make_ble_address(u8 * ble_address, u8 * edr_address);
        /* bt_ble_adv_enable(0); */
        u8 tmp_ble_addr[6] = {0};
        lib_make_ble_address(tmp_ble_addr, comm_addr);
        le_controller_set_mac(tmp_ble_addr);//将ble广播地址改成公共地址
        bt_update_mac_addr(comm_addr);
        /* bt_ble_adv_enable(1); */


        /*新的连接，公共地址改变了，要重新将新的地址广播出去*/
        if (tws_api_get_role() == TWS_ROLE_MASTER) {
            printf("\nNew Connect Master!!!\n\n");
            ble_app_disconnect();
            bt_ble_adv_enable(0);
            bt_ble_adv_enable(1);
        } else {
            printf("\nConnect Slave!!!\n\n");
            /*从机ble关掉*/
            ble_app_disconnect();
            bt_ble_adv_enable(0);
        }
    }

    return 0;
}

int tme_earphone_state_enter_soft_poweroff()
{
    extern void bt_ble_exit(void);
    bt_ble_exit();
    return 0;
}

static int tme_bt_connction_status_event_handler(struct bt_event *bt)
{
    switch (bt->event) {
    case BT_STATUS_SECOND_CONNECTED:
    case BT_STATUS_FIRST_CONNECTED:
        TME_set_edr_connected(1);
        break;

    case BT_STATUS_FIRST_DISCONNECT:
    case BT_STATUS_SECOND_DISCONNECT:
        ble_app_disconnect();
        break;

    default:
        break;

    }

    return 0;
}

int tme_adv_bt_status_event_handler(struct bt_event *bt)
{
    return 0;
}


int tme_adv_hci_event_handler(struct bt_event *bt)
{

    return 0;
}

void tme_bt_tws_event_handler(struct bt_event *bt)
{
    int role = bt->args[0];
    int phone_link_connection = bt->args[1];
    int reason = bt->args[2];

    switch (bt->event) {
    case TWS_EVENT_CONNECTED:
        //bt_ble_adv_enable(1);
        if (tws_api_get_role() == TWS_ROLE_SLAVE) {
            //master enable
            printf("\nConnect Slave!!!\n\n");
            /*从机ble关掉*/
            ble_app_disconnect();
            bt_ble_adv_enable(0);
        }
        if (BT_STATUS_WAITINT_CONN != get_bt_connect_status()) {
            TME_set_edr_connected(1);
        }
        break;
    case TWS_EVENT_PHONE_LINK_DETACH:
        /*
         * 跟手机的链路LMP层已完全断开, 只有tws在连接状态才会收到此事件
         */
        break;
    case TWS_EVENT_CONNECTION_DETACH:
        /*
         * TWS连接断开
         */
        if (app_var.goto_poweroff_flag) {
            break;
        }
        if (get_app_connect_type() == 0) {
            printf("\ntws detach to open ble~~~\n\n");
            bt_ble_adv_enable(1);
        }
        set_ble_connect_type(TYPE_NULL);

        break;
    case TWS_EVENT_SYNC_FUN_CMD:
        break;
    case TWS_EVENT_ROLE_SWITCH:
        if (role == TWS_ROLE_MASTER) {
            if (JL_spp_fw_ready(NULL)) {
                tme_spp_disconnect(NULL);
                bt_ble_adv_enable(1);
            }

        } else {
            if (JL_spp_fw_ready(NULL)) {
                tme_spp_disconnect(NULL);
            }
        }
        break;

    default:
        break;
    }

#if OTA_TWS_SAME_TIME_ENABLE
    tws_ota_app_event_deal(bt->event);
#endif
}

int tme_sys_event_handler_specific(struct sys_event *event)
{
    switch (event->type) {
    case SYS_BT_EVENT:
        if ((u32)event->arg == SYS_BT_EVENT_TYPE_CON_STATUS) {
            tme_bt_connction_status_event_handler(&event->u.bt);
        } else if ((u32)event->arg == SYS_BT_EVENT_TYPE_HCI_STATUS) {

        }
#if TCFG_USER_TWS_ENABLE
        else if (((u32)event->arg == SYS_BT_EVENT_FROM_TWS)) {
            tme_bt_tws_event_handler(&event->u.bt);
        }
#endif
#if OTA_TWS_SAME_TIME_ENABLE
        else if (((u32)event->arg == SYS_BT_OTA_EVENT_TYPE_STATUS)) {
            bt_ota_event_handler(&event->u.bt);
        }
#endif
        break;
    case SYS_DEVICE_EVENT:
        break;
    }

    return 0;
}

int user_spp_state_specific(u8 packet_type)
{

    switch (packet_type) {
    case 1:
        ble_app_disconnect();
        bt_ble_adv_enable(0);
        set_app_connect_type(TYPE_SPP);
        break;
    case 2:

        set_app_connect_type(TYPE_NULL);

#if TCFG_USER_TWS_ENABLE
        if (!(tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED)) {
            ble_module_enable(1);
        } else {
            if (tws_api_get_role() == TWS_ROLE_MASTER) {
                ble_module_enable(1);
            }
        }
#else
        ble_module_enable(1);
#endif

        break;
    }
    return 0;
}


int tme_earphone_state_init()
{
    tme_spp_init();
    tme_ble_init();
    spp_data_deal_handle_register(user_spp_data_handler);

    return 0;
}





#endif
