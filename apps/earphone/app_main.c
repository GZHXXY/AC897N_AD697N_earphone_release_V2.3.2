#include "system/includes.h"
/*#include "btcontroller_config.h"*/
#include "btstack/btstack_task.h"
#include "app_config.h"
#include "app_action.h"
#include "asm/pwm_led.h"
#include "tone_player.h"
#include "ui_manage.h"
#include "gpio.h"
#include "app_main.h"
#include "asm/charge.h"
#include "update.h"
#include "app_power_manage.h"
#include "audio_config.h"
#include "app_charge.h"
#include "bt_profile_cfg.h"
#include "dev_manager/dev_manager.h"
#include "update_loader_download.h"

#ifndef CONFIG_MEDIA_NEW_ENABLE
#include "audio_dec_server.h"
#endif

#if TCFG_USER_TWS_ENABLE
#include "bt_tws.h"
#endif

#define LOG_TAG_CONST       APP
#define LOG_TAG             "[APP]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"




/*任务列表 */
const struct task_info task_info_table[] = {
    {"app_core",            1,     768,   256  },
    {"sys_event",           7,     256,   0    },
    {"systimer",		    7,	   128,   0	   },
    {"btctrler",            4,     512,   384  },
    {"btencry",             1,     512,   128  },
    {"tws",                 5,     512,   128  },
#if (BT_FOR_APP_EN)
    {"btstack",             3,     1024,  256  },
#else
    {"btstack",             3,     768,   256  },
#endif
    {"audio_dec",           5,     800,   128  },
    /*
     *为了防止dac buf太大，通话一开始一直解码，
     *导致编码输入数据需要很大的缓存，这里提高编码的优先级
     */
    {"audio_enc",           6,     768,   128  },
    {"aec",					2,	   512,   128  },
#if (TUYA_DEMO_EN)
    {"user_deal",           7,     512,   512   },//定义线程 tuya任务调度    {"dw_update",           2,     256,   128  },
    {"dw_update",           2,     256,   128  },
#endif

#ifndef CONFIG_256K_FLASH
    {"aec_dbg",				3,	   128,   128  },
    {"update",				1,	   512,   0    },
    {"tws_ota",				2,	   256,   0    },
    {"tws_ota_msg",			2,	   256,   128  },
    {"dw_update",		 	2,	   256,   128  },
    {"rcsp_task",		    2,	   640,	  128  },
    {"audio_capture",       4,     512,   256  },
    {"data_export",         5,     512,   256  },
    {"anc",                 3,     512,   128  },
#endif

#if TCFG_GX8002_NPU_ENABLE
    {"gx8002",              2,     256,   64   },
#endif /* #if TCFG_GX8002_NPU_ENABLE */
#if (TME_EN)
    {"tme",  	            2,     512,   64   },
#endif
#if (XM_MMA_EN)
    {"xm_mma",  	        2,     512,   64   },
#endif

#if TCFG_KWS_VOICE_RECOGNITION_ENABLE
    {"kws",                 2,     256,   64   },
#endif /* #if TCFG_KWS_VOICE_RECOGNITION_ENABLE */
#if TCFG_KEY_TONE_EN
    {"key_tone",            5,     256,   32   },
#endif
    {"usb_msd",           	1,     512,   128   },
#if(USER_UART_UPDATE_ENABLE)
    {"uart_update",         7,     0,   256,   128  },
#endif
    {0, 0},
};


APP_VAR app_var;

/*
 * 2ms timer中断回调函数
 */
void timer_2ms_handler()
{

}

void app_var_init(void)
{
    memset((u8 *)&bt_user_priv_var, 0, sizeof(BT_USER_PRIV_VAR));
    app_var.play_poweron_tone = 1;

}



void app_earphone_play_voice_file(const char *name);

void clr_wdt(void);

void check_power_on_key(void)
{
    u32 delay_10ms_cnt = 0;

    while (1) {
        clr_wdt();
        os_time_dly(1);

        extern u8 get_power_on_status(void);
        if (get_power_on_status()) {
            log_info("+");
            delay_10ms_cnt++;
            if (delay_10ms_cnt > 70) {
                return;
            }
        } else {
            log_info("-");
            delay_10ms_cnt = 0;
            log_info("enter softpoweroff\n");
            power_set_soft_poweroff();
        }
    }
}


extern int cpu_reset_by_soft();
extern int audio_dec_init();
extern int audio_enc_init();



__attribute__((weak))
u8 get_charge_online_flag(void)
{
    return 0;
}

/*充电拔出,CPU软件复位, 不检测按键，直接开机*/
static void app_poweron_check(int update)
{
#if (CONFIG_BT_MODE == BT_NORMAL)
    if (!update && cpu_reset_by_soft()) {
        app_var.play_poweron_tone = 0;
        return;
    }

#if TCFG_CHARGE_OFF_POWERON_NE
    if (is_ldo5v_wakeup()) {
        app_var.play_poweron_tone = 0;
        return;
    }
#endif
#ifdef CONFIG_RELEASE_ENABLE
#if TCFG_POWER_ON_NEED_KEY
    check_power_on_key();
#endif
#endif

#endif
}

void app_main()
{
    int update = 0;
    u32 addr = 0, size = 0;
    struct intent it;


    log_info("app_main\n");

#ifdef CONFIG_MEDIA_NEW_ENABLE
    /*解码器*/
    audio_dec_init();
    audio_enc_init();
#endif

    if (!UPDATE_SUPPORT_DEV_IS_NULL()) {
        update = update_result_deal();
    }

    app_var_init();

#if TCFG_MC_BIAS_AUTO_ADJUST
    mc_trim_init(update);
#endif/*TCFG_MC_BIAS_AUTO_ADJUST*/

    if (get_charge_online_flag()) {

#if(TCFG_SYS_LVD_EN == 1)
        vbat_check_init();
#endif

        init_intent(&it);
        it.name = "idle";
        it.action = ACTION_IDLE_MAIN;
        start_app(&it);
    } else {
        check_power_on_voltage();

        app_poweron_check(update);

        ui_manage_init();
        ui_update_status(STATUS_POWERON);

        init_intent(&it);
        it.name = "earphone";
        it.action = ACTION_EARPHONE_MAIN;
        start_app(&it);
    }
}

int __attribute__((weak)) eSystemConfirmStopStatus(void)
{
    /* 系统进入在未来时间里，无任务超时唤醒，可根据用户选择系统停止，或者系统定时唤醒(100ms)，或自己指定唤醒时间 */
    //1:Endless Sleep
    //0:100 ms wakeup
    //other: x ms wakeup
    if (get_charge_full_flag()) {
        /* log_i("Endless Sleep"); */
        power_set_soft_poweroff();
        return 1;
    } else {
        /* log_i("100 ms wakeup"); */
        return 0;
    }
}

#if(CONFIG_CPU_BR30)
int *__errno()
{
    static int err;
    return &err;
}
#endif



