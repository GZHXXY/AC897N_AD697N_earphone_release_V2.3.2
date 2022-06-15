#include "system/includes.h"
#include "music/music_player.h"
#include "music/breakpoint.h"
#include "app_action.h"
#include "app_main.h"
#include "earphone.h"
#include "key_event_deal.h"
#include "audio_config.h"
#include "bt_background.h"
#include "default_event_handler.h"
#include "app_online_cfg.h"


#if CONFIG_APP_MUSIC_ENABLE

struct app_music_var {
    u8 inited;
    u8 music_from_tws;
    struct __breakpoint *breakpoint;
};

static struct app_music_var g_music;

extern const u8 music_key_table[][KEY_EVENT_MAX];

static u8 music_idle_query()
{
    return 0;
}
REGISTER_LP_TARGET(music_lp_target) = {
    .name = "music",
    .is_idle = music_idle_query,
};

static int music_device_init()
{
    char *dev_name = "sd0";
    dev_manager_var_init();
    dev_manager_add(dev_name);
    return 0;
}
late_initcall(music_device_init);

static void music_save_breakpoint(int save_dec_bp)
{
    char *logo = music_player_get_dev_cur();

    ///save breakpoint, 只保存文件信息
    if (music_player_get_playing_breakpoint(g_music.breakpoint, save_dec_bp) == true) {
        breakpoint_vm_write(g_music.breakpoint, logo);
    }
}

static void music_player_play_success(void *priv, int parm)
{
    music_save_breakpoint(0);
}

static void music_player_play_end(void *priv, int parm)
{
    int err = music_player_end_deal(parm);
    if (err == MUSIC_PLAYER_ERR_DEV_READ) {
        music_save_breakpoint(1);
        music_player_stop(1);
    }
}

static int music_player_scandisk_break(void)
{
    return 0;
}

static const struct __player_cb music_player_callback = {
    .start 		= music_player_play_success,
    .end   		= music_player_play_end,
};

static const struct __scan_callback scan_cb = {
    .enter = NULL,
    .exit = NULL,
    .scan_break = music_player_scandisk_break,
};

static int app_music_init()
{
    int err = 0, ret = 0;
    char *dev_name = "sd0";
    struct __player_parm parm;

    if (g_music.music_from_tws) {
        return 0;
    }

    parm.cb         = &music_player_callback;
    parm.scan_cb    = &scan_cb;
    music_player_creat(NULL, &parm);

    ///获取断点句柄， 后面所有断点读/写都需要用到
    g_music.breakpoint = breakpoint_handle_creat();

    ret = syscfg_read(CFG_MUSIC_MODE, &app_var.cycle_mode, 1);
    if (ret < 0) {
        printf("read music play mode err\n");
    }
    if (app_var.cycle_mode >= FCYCLE_MAX || app_var.cycle_mode == 0) {
        app_var.cycle_mode = FCYCLE_ALL;
    }

    if (true == breakpoint_vm_read(g_music.breakpoint, dev_name)) {
        err = music_player_play_by_breakpoint(dev_name, g_music.breakpoint);
    } else {
        err = music_player_play_first_file(dev_name);
    }

    printf("music_play: err = %d\n", err);

    if (err > 1) {
        breakpoint_handle_destroy(&g_music.breakpoint);
        music_player_destroy();
        return err;
    }

    return 0;
}

void app_music_exit()
{
    if (g_music.music_from_tws) {
        return;
    }
    music_save_breakpoint(1);
    music_player_stop(1);
    breakpoint_handle_destroy(&g_music.breakpoint);
    music_player_destroy();
}

static int app_music_key_event_handler(struct sys_event *event)
{
    struct key_event *key = &event->u.key;
#if TCFG_SD0_ENABLE
    int key_event = KEY_MUSIC_PP;
#else
    int key_event = music_key_table[key->value][key->event];
#endif

    switch (key_event) {
    case KEY_MUSIC_PP:
        music_player_pp();
        break;
    case KEY_MUSIC_NEXT:
        if (g_music.music_from_tws == 0) {
            music_player_play_next();
        }
        break;
    case KEY_MUSIC_PREV:
        if (g_music.music_from_tws == 0) {
            music_player_play_prev();
        }
        break;
    case KEY_MODE_SWITCH:
        bt_switch_to_foreground(ACTION_BY_KEY_MODE, 1);
        break;
    case KEY_VOL_UP:
    case KEY_VOL_DOWN:
    case KEY_POWEROFF:
    case KEY_POWEROFF_HOLD:
        app_earphone_key_event_handler(event);
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
    switch (event->type) {
    case SYS_KEY_EVENT:
        /*
         * 普通按键消息处理
         */
        return app_music_key_event_handler(event);
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
#if TCFG_ONLINE_ENABLE
        } else if ((u32)event->arg == DEVICE_EVENT_FROM_CI_UART) {
            ci_data_rx_handler(CI_UART);
#if TCFG_USER_TWS_ENABLE
        } else if ((u32)event->arg == DEVICE_EVENT_FROM_CI_TWS) {
            ci_data_rx_handler(CI_TWS);
#endif
#endif
        }
        break;
    default:
        break;
    }

    default_event_handler(event);

    return false;
}

static int state_machine(struct application *app, enum app_state state,
                         struct intent *it)
{
    int err = 0;

    switch (state) {
    case APP_STA_CREATE:
        break;
    case APP_STA_START:
        if (!it) {
            break;
        }
        switch (it->action) {
        case ACTION_MUSIC_MAIN:
            g_music.music_from_tws = 0;
            err = app_music_init();
            break;
        case ACTION_MUSIC_TWS_RX:
            g_music.music_from_tws = 1;
            break;
        }
        break;
    case APP_STA_PAUSE:
        app_music_exit();
        break;
    case APP_STA_RESUME:
        app_music_init();
        break;
    case APP_STA_STOP:
        app_music_exit();
        break;
    case APP_STA_DESTROY:
        break;
    }

    return err;
}

static const struct application_operation app_music_ops = {
    .state_machine  = state_machine,
    .event_handler 	= event_handler,
};


/*
 * 注册music模式
 */
REGISTER_APPLICATION(app_music) = {
    .name 	= "music",
    .action	= ACTION_MUSIC_MAIN,
    .ops 	= &app_music_ops,
    .state  = APP_STA_DESTROY,
};





#endif

