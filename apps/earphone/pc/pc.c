#include "system/includes.h"
#include "music/music_player.h"
#include "music/breakpoint.h"
#include "app_action.h"
#include "key_event_deal.h"
#include "audio_config.h"
#include "bt_background.h"
#include "default_event_handler.h"
#include "usb/device/usb_stack.h"

#if TCFG_PC_ENABLE


struct app_pc_var {
    u8 inited;
    u8 idle;
};

static struct app_pc_var g_pc;

static u8 pc_key_table[][KEY_EVENT_MAX] = {
    // SHORT           LONG              HOLD
    // UP              DOUBLE           TRIPLE
    {
        KEY_MUSIC_PP,      KEY_POWEROFF,       KEY_POWEROFF_HOLD,
        KEY_NULL,          KEY_MODE_SWITCH,    KEY_NULL
    },

    {
        KEY_MUSIC_NEXT,    KEY_VOL_UP,         KEY_VOL_UP,
        KEY_NULL,          KEY_NULL,           KEY_NULL
    },

    {
        KEY_MUSIC_PREV,    KEY_VOL_DOWN,       KEY_VOL_DOWN,
        KEY_NULL,          KEY_NULL,           KEY_NULL
    },
};

static u8 pc_idle_query()
{
    return g_pc.idle;
}
REGISTER_LP_TARGET(pc_lp_target) = {
    .name = "pc",
    .is_idle = pc_idle_query,
};



static void app_pc_init()
{
    r_printf("%s() %d", __func__, __LINE__);
    g_pc.idle = 0;
#if TCFG_PC_ENABLE
    clk_set("sys", 96000000); //提高系统时钟,优化usb出盘符速度
    usb_start();
#endif
}

static void app_pc_exit()
{
#if TCFG_PC_ENABLE
    usb_stop();
#endif
    g_pc.idle = 1;
}

static int app_pc_key_event_handler(struct sys_event *event)
{
    struct key_event *key = &event->u.key;
    int key_event = pc_key_table[key->value][key->event];

    switch (key_event) {
    case KEY_MUSIC_PP:
        break;
    case KEY_MUSIC_NEXT:
        break;
    case KEY_MUSIC_PREV:
        break;
    case KEY_VOL_UP:
        break;
    case KEY_VOL_DOWN:
        break;
    case KEY_MODE_SWITCH:
        bt_switch_to_foreground(ACTION_BY_KEY_MODE, 1);
        break;
    default:
        break;
    }

    return false;
}


/*
 * 系统事件处理函数
 */
static int event_handler(struct application *app, struct sys_event *event)
{
    /*if (SYS_EVENT_REMAP(event)) {
        g_printf("****SYS_EVENT_REMAP**** \n");
        return 0;
    }*/

    switch (event->type) {
    case SYS_KEY_EVENT:
        /*
         * 普通按键消息处理
         */
        return app_pc_key_event_handler(event);
    case SYS_BT_EVENT:
        /*
         * 蓝牙事件处理
         */
        break;
    case SYS_DEVICE_EVENT:
        /*
         * 系统设备事件处理
         */
        if ((u32)event->arg == DRIVER_EVENT_FROM_SD0) {
        }
        break;
    default:
        return false;
    }

    default_event_handler(event);

    return false;
}

static int state_machine(struct application *app, enum app_state state,
                         struct intent *it)
{
    switch (state) {
    case APP_STA_CREATE:
        break;
    case APP_STA_START:
        if (!it) {
            break;
        }
        switch (it->action) {
        case ACTION_PC_MAIN:
            app_pc_init();
            break;
        }
        break;
    case APP_STA_PAUSE:
        break;
    case APP_STA_RESUME:
        break;
    case APP_STA_STOP:
        app_pc_exit();
        break;
    case APP_STA_DESTROY:
        break;
    }

    return 0;
}

static const struct application_operation app_pc_ops = {
    .state_machine  = state_machine,
    .event_handler 	= event_handler,
};


/*
 * 注册pc模式
 */
REGISTER_APPLICATION(app_pc) = {
    .name 	= "pc",
    .action	= ACTION_PC_MAIN,
    .ops 	= &app_pc_ops,
    .state  = APP_STA_DESTROY,
};


#endif

