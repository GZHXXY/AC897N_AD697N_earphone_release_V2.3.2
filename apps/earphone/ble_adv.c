#include "app_config.h"


#if (TCFG_BLE_DEMO_SELECT == DEF_BLE_DEMO_ADV)

#include "earphone.h"
#include "app_main.h"
#include "include/bt_ble.h"
#include "bt_common.h"
#include "btstack/avctp_user.h"
#include "system/includes.h"
#include "bt_tws.h"

#define LOG_TAG             "[BLE-ADV]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

typedef struct {
    u8 miss_flag: 1;
    u8 exchange_bat: 2;
    u8 poweron_flag: 1;
    u8 reserver: 4;
} icon_ctl_t;

static icon_ctl_t ble_icon_contrl;


int adv_earphone_state_set_page_scan_enable()
{
#if (TCFG_USER_TWS_ENABLE == 0)
    bt_ble_icon_open(ICON_TYPE_INQUIRY);
#elif (CONFIG_NO_DISPLAY_BUTTON_ICON || !TCFG_CHARGESTORE_ENABLE)
    if (tws_api_get_role() == TWS_ROLE_MASTER) {
        printf("switch_icon_ctl11...\n");
        bt_ble_icon_open(ICON_TYPE_INQUIRY);
    }
#endif
    return 0;
}

int adv_earphone_state_get_connect_mac_addr()
{
    return 0;
}

int adv_earphone_state_cancel_page_scan()
{
#if (TCFG_USER_TWS_ENABLE == 1)
#if (CONFIG_NO_DISPLAY_BUTTON_ICON || !TCFG_CHARGESTORE_ENABLE)
    if (tws_api_get_role() == TWS_ROLE_MASTER) {
        if (ble_icon_contrl.miss_flag) {
            ble_icon_contrl.miss_flag = 0;
            puts("ble_icon_contrl.miss_flag...\n");
        } else {
            printf("switch_icon_ctl00...\n");
            bt_ble_icon_open(ICON_TYPE_INQUIRY);
        }
    }
#endif
#endif
    return 0;
}

int adv_earphone_state_tws_init(int paired)
{
    memset(&ble_icon_contrl, 0, sizeof(icon_ctl_t));
    ble_icon_contrl.poweron_flag = 1;

    if (paired) {
        if (tws_api_get_role() == TWS_ROLE_MASTER) {
            bt_ble_set_control_en(1);
        } else {
            //slave close
            bt_ble_set_control_en(0);
        }
    } else {

    }

    return 0;
}

int adv_earphone_state_tws_connected(int first_pair, u8 *comm_addr)
{
    if (first_pair) {
        bt_ble_icon_set_comm_address(comm_addr);
    }
    return 0;
}

int adv_earphone_state_enter_soft_poweroff()
{
#if (!TCFG_CHARGESTORE_ENABLE)
    //?????????????????????????????????????????????
    if (bt_ble_icon_get_adv_state() != ADV_ST_NULL &&
        bt_ble_icon_get_adv_state() != ADV_ST_END) {
        bt_ble_icon_close(1);
        os_time_dly(50);//???????????????????????????????????????
    }
#endif
    bt_ble_exit();
    return 0;
}



int ble_adv_hci_event_handler(struct bt_event *bt)
{
    switch (bt->event) {
    case HCI_EVENT_CONNECTION_COMPLETE:
        switch (bt->value) {
        case ERROR_CODE_PIN_OR_KEY_MISSING:
#if (CONFIG_NO_DISPLAY_BUTTON_ICON && TCFG_CHARGESTORE_ENABLE)
            //??????????????????
            if (bt_ble_icon_get_adv_state() == ADV_ST_RECONN) {
                //????????????
                bt_ble_icon_open(ICON_TYPE_INQUIRY);
            }
#endif
            break;
        }
        break;
    }

    return 0;
}

void ble_adv_bt_tws_event_handler(struct bt_event *bt)
{
    int role = bt->args[0];
    int phone_link_connection = bt->args[1];
    int reason = bt->args[2];

    switch (bt->event) {
    case TWS_EVENT_CONNECTED:
        bt_ble_icon_slave_en(1);
        if (tws_api_get_role() == TWS_ROLE_MASTER) {
            //master enable
            log_info("master do icon_open\n");
            bt_ble_set_control_en(1);

            if (phone_link_connection) {
                bt_ble_icon_open(ICON_TYPE_RECONNECT);
            } else {
#if (TCFG_CHARGESTORE_ENABLE && !CONFIG_NO_DISPLAY_BUTTON_ICON)
                bt_ble_icon_open(ICON_TYPE_RECONNECT);
#else
                if (ble_icon_contrl.poweron_flag) { //????????????
                    if (bt_user_priv_var.auto_connection_counter > 0) {
                        //?????????????????????
                        /* g_printf("ICON_TYPE_RECONNECT"); */
                        /* bt_ble_icon_open(ICON_TYPE_RECONNECT); //?????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????*/
                    } else {
                        //???????????????????????????
                        /* g_printf("ICON_TYPE_INQUIRY"); */
                        bt_ble_icon_open(ICON_TYPE_INQUIRY);
                    }

                }
#endif
            }
        } else {
            //slave disable
            bt_ble_set_control_en(0);
        }
        ble_icon_contrl.poweron_flag = 0;
        break;
    case TWS_EVENT_CONNECTION_TIMEOUT:
        /*
        * TWS????????????
        */
        bt_ble_icon_slave_en(0);
        break;
    case TWS_EVENT_PHONE_LINK_DETACH:
        /*
         * ??????????????????LMP??????????????????, ??????tws????????????????????????????????????
         */
        if (reason == 0x0b) {
            //CONNECTION ALREADY EXISTS
            ble_icon_contrl.miss_flag = 1;
        } else {
            ble_icon_contrl.miss_flag = 0;
        }
        break;
    }
}

int adv_sys_event_handler_specific(struct sys_event *event)
{
    switch (event->type) {
    case SYS_BT_EVENT:
        if ((u32)event->arg == SYS_BT_EVENT_TYPE_CON_STATUS) {
        } else if ((u32)event->arg == SYS_BT_EVENT_TYPE_HCI_STATUS) {
            ble_adv_hci_event_handler(&event->u.bt);
        }
#if TCFG_USER_TWS_ENABLE
        else if (((u32)event->arg == SYS_BT_EVENT_FROM_TWS)) {
            ble_adv_bt_tws_event_handler(&event->u.bt);
        }
#endif
        break;
    }

    return 0;
}

int adv_earphone_state_init()
{
    return 0;
}

int adv_earphone_state_sniff(u8 state)
{
    bt_ble_icon_state_sniff(state);
    return 0;
}

int adv_earphone_state_role_switch(u8 role)
{
    bt_ble_icon_role_switch(role);
    return 0;
}





#endif

