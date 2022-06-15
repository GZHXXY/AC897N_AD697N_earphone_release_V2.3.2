#include "default_event_handler.h"
#include "app_action.h"
#include "bt_background.h"
#include "earphone.h"
#include "dev_manager/dev_manager.h"
#include "app_config.h"
#include "btstack/avctp_user.h"
#include "bt_tws.h"
#include "app_main.h"
#include "app_music.h"
#include "key_event_deal.h"

#if TCFG_APP_MUSIC_EN

#if TCFG_DEC2TWS_ENABLE
#include "audio_dec/audio_dec_file.h"
#endif

extern u8 key_table[KEY_NUM_MAX][KEY_EVENT_MAX];
extern int pc_device_event_handler(struct sys_event *event);



int switch_to_music_app(int action)
{
    struct intent it;

    if (action == ACTION_MUSIC_MAIN) {
        /* 卡不在线不切到music模式,
         * 防止TWS之间所处的模式不同导致A2DP播歌不同步
         */
        if (!dev_online("sd0")) {
            return -ENODEV;
        }
    }

    init_intent(&it);
    it.name = "music";
    it.action = action;
    return start_app(&it);
}

void switch_to_prev_app()
{
    struct intent it;

    init_intent(&it);
    it.action = ACTION_BACK;
    start_app(&it);
}

void switch_to_earphone_app()
{
    struct intent it;

    init_intent(&it);
    it.action = ACTION_BACK;
    start_app(&it);
}

void switch_to_pc_app()
{
    struct intent it;

    /* 退出bt和music模式 */
    if (bt_in_background()) {
        init_intent(&it);
        it.action = ACTION_BACK;
        start_app(&it);
    }
    init_intent(&it);
    it.action = ACTION_BACK;
    start_app(&it);


    /* 切到PC模式 */
    init_intent(&it);
    it.name = "pc";
    it.action = ACTION_PC_MAIN;
    start_app(&it);
}

static void sd_event_handler(struct device_event *evt)
{
    if (evt->event == DEVICE_EVENT_IN) {
        r_printf("sd0_in\n");
        if (app_var.have_mass_storage == 0) {
            app_var.have_mass_storage = 1;
            syscfg_write(CFG_HAVE_MASS_STORAGE, &app_var.have_mass_storage, 1);
        }
        dev_manager_mount("sd0");
        if (bt_in_background()) {
            switch_to_music_app(ACTION_MUSIC_MAIN);
        }
    } else {
        r_printf("sd0_out\n");
        dev_manager_unmount("sd0");
    }
}

static int default_earphone_key_event_handler(struct sys_event *event)
{
    struct key_event *key = &event->u.key;
    u8 key_event = key_table[key->value][key->event];

    switch (key_event) {
    case KEY_MODE_SWITCH:
        puts("key_mode_switch\n");
        switch_to_music_app(ACTION_MUSIC_MAIN);
        break;
    }

    return false;
}

static int default_tws_event_handler(struct bt_event *evt)
{
#if TCFG_DEC2TWS_ENABLE
    int role = evt->args[0];
    int phone_link_connection = evt->args[1];
    int state = evt->args[2];

    switch (evt->event) {
    case TWS_EVENT_CONNECTED:
        tws_api_auto_role_switch_disable();
        if (state & (TWS_STA_ESCO_OPEN | TWS_STA_SBC_OPEN)) {
            if (bt_in_background()) {
                bt_switch_to_foreground(ACTION_DO_NOTHING, 1);
            }
            break;
        }
        if (file_dec_get_source() == FILE_FROM_LOCAL) {
            send_local_media_dec_open_cmd();
        } else {
            if (role == TWS_ROLE_MASTER) {
                if (switch_to_music_app(ACTION_MUSIC_MAIN) == 0) {
                    break;
                }
            }
            if (bt_in_background()) {
                bt_switch_to_foreground(ACTION_DO_NOTHING, 1);
            }
        }
        break;
    case TWS_EVENT_CONNECTION_TIMEOUT:
        if (file_dec_get_source() == -EINVAL) {
            switch_to_music_app(ACTION_MUSIC_MAIN);
        }
        break;
    case TWS_EVENT_MONITOR_START:
        if (role == TWS_ROLE_MASTER) {
            if (!app_var.have_mass_storage || file_dec_get_source() == FILE_FROM_TWS) {
                tws_conn_switch_role();
            }
        }
        break;
    case TWS_EVENT_CONNECTION_DETACH:
        break;
    case TWS_EVENT_LOCAL_MEDIA_START:
        if (role == TWS_ROLE_MASTER) {
            if (phone_link_connection) {
                tws_conn_switch_role();
            }
            if (tws_api_get_tws_state() & (TWS_STA_ESCO_OPEN | TWS_STA_SBC_OPEN)) {
                break;
            }
        }
        if (file_dec_get_source() == FILE_FROM_LOCAL) {
            if (role == TWS_ROLE_MASTER) {
                break;
            }
            app_music_exit();
        }

        switch_to_music_app(ACTION_MUSIC_TWS_RX);
        tws_local_media_dec_open(evt->args + 2);
        sys_auto_shut_down_disable();
        break;
    case TWS_EVENT_LOCAL_MEDIA_STOP:
        tws_local_media_dec_close();
        sys_auto_shut_down_enable();
        if (tws_api_get_tws_state() & TWS_STA_SIBLING_DISCONNECTED) {
            struct application *app = get_current_app();
            if (app && app->action != ACTION_PC_MAIN) {
                bt_switch_to_foreground(ACTION_DO_NOTHING, 1);
            }
        }
        break;
    default:
        return false;
    }

#endif
    return true;
}

void default_event_handler(struct sys_event *event)
{
    /*puts("default_event_handler\n");*/
    if (bt_in_background()) {
        bt_in_background_event_handler(event);
    }

    switch (event->type) {
    case SYS_KEY_EVENT:
        if (!bt_in_background()) {
            default_earphone_key_event_handler(event);
        }
        break;
    case SYS_BT_EVENT:
        if ((u32)event->arg == SYS_BT_EVENT_TYPE_CON_STATUS) {
            struct bt_event *bt = &event->u.bt;
            if (bt->event == BT_STATUS_SCO_STATUS_CHANGE) {
                if (bt->value == 0xff) {
                    app_core_back_to_prev_app();
                }
            }
        }
#if TCFG_USER_TWS_ENABLE
        else if (((u32)event->arg == SYS_BT_EVENT_FROM_TWS)) {
            default_tws_event_handler(&event->u.bt);
        }
#endif
        break;

    case SYS_DEVICE_EVENT:
        /* 系统设备事件处理 */
#if TCFG_SD0_ENABLE
        if ((u32)event->arg == DRIVER_EVENT_FROM_SD0) {
            sd_event_handler(&event->u.dev);
            break;
        }
#endif

#if TCFG_PC_ENABLE
        int ret = pc_device_event_handler(event);
        if (ret) {
            if (event->u.dev.event == DEVICE_EVENT_IN) {
                switch_to_pc_app();
            } else {
                cpu_reset();
            }
        }
#endif
        break;
    default:
        break;
    }

}

#else

void default_event_handler(struct sys_event *event)
{

}

#endif

