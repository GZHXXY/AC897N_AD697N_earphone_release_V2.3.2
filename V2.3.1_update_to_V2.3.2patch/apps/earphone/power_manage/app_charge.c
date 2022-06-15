#include "app_config.h"
#include "asm/charge.h"
#include "asm/pwm_led.h"
#include "asm/power_interface.h"
#include "ui_manage.h"
#include "system/event.h"
#include "system/app_core.h"
#include "system/includes.h"
#include "app_action.h"
#include "asm/wdt.h"
#include "app_power_manage.h"
#include "app_chargestore.h"
#include "btstack/avctp_user.h"
#include "app_action.h"
#include "app_main.h"
#include "bt_tws.h"
#include "usb/otg.h"
#include "bt_common.h"

#if TCFG_AUDIO_ANC_ENABLE
#include "audio_anc.h"
#endif

#define LOG_TAG_CONST       APP_CHARGE
#define LOG_TAG             "[APP_CHARGE]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

#if TCFG_ANC_BOX_ENABLE
#include "app_ancbox.h"
#endif

extern void sys_enter_soft_poweroff(void *priv);
extern void gSensor_wkupup_enable(void);
extern void gSensor_wkupup_disable(void);
extern u32 dual_bank_update_exist_flag_get(void);
extern u32 classic_update_task_exist_flag_get(void);
extern u8 testbox_get_ex_enter_storage_mode_flag(void);
extern void lp_touch_key_disable(void);

#if TCFG_CHARGE_ENABLE

#define TCFG_CHARGE_CHECK_SET_LDOIN_PINR

static u8 charge_full_flag = 0;
#if TCFG_TEST_BOX_ENABLE || TCFG_ANC_BOX_ENABLE
static int charge_err_timer = 0;
static int ldo5v_keep_timer = 0;
#endif
static u8 delay_timeout = 20;
static int ldo5v_in_reset_timer = 0;
static void ldo5v_in_reset_delay(void)
{
    int deal_state = tws_api_get_tws_state();
    u8 cpu_reset_flag = 1;
    if (delay_timeout) {
        delay_timeout--;
    }
    if (deal_state & TWS_STA_SIBLING_CONNECTED && deal_state & TWS_STA_PHONE_CONNECTED && !(deal_state & TWS_STA_MONITOR_START)) {
        //tws已连接 手机已连接 从机未监听手机
        cpu_reset_flag = 0;
    } else {
        if (deal_state & TWS_STA_SIBLING_CONNECTED && deal_state & TWS_STA_ESCO_OPEN && !get_esco_coder_busy_flag()) {
            //手机有请求但没建立ESCO链路
            cpu_reset_flag = 0;
        }
    }
    printf("----------------ldo5v_in_reset_delay tws state:%x, delay_timeout:%d, cpu_reset_flag:%d\n", deal_state, delay_timeout, cpu_reset_flag);
    if (delay_timeout == 0 || cpu_reset_flag == 1) {
        if (ldo5v_in_reset_timer) {
            sys_timer_del(ldo5v_in_reset_timer);
            ldo5v_in_reset_timer = 0;
        }
        sys_enter_soft_poweroff((void *)1);
    }
}

extern void power_event_to_user(u8 event);

u8 get_charge_full_flag(void)
{
    return charge_full_flag;
}

void charge_start_deal(void)
{
    log_info("%s\n", __FUNCTION__);

    power_set_mode(PWR_LDO15);

    struct application *app;
    app = get_current_app();
    //在IDLE时才开启充电UI
    if (app && (!strcmp(app->name, APP_NAME_IDLE))) {
        ui_update_status(STATUS_CHARGE_START);
    }
}


static void charge_err_delay_deal(void *priv)
{
    log_info("%s\n", __func__);

#if TCFG_TEST_BOX_ENABLE
    charge_err_timer = 0;
    if (chargestore_get_testbox_status()) {
        log_info("testbox online!\n");
        return;
    }
#endif

#if TCFG_ANC_BOX_ENABLE
    charge_err_timer = 0;
    if (ancbox_get_status()) {
        log_info("ancbox online!\n");
        return;
    }
#endif

    struct application *app;
    app = get_current_app();
    if (app) {
        if (strcmp(app->name, APP_NAME_IDLE)) {
            sys_enter_soft_poweroff((void *)2);
        }
    }

    pwm_led_mode_set(PWM_LED_ALL_OFF);
    os_time_dly(30);
    pwm_led_mode_set(PWM_LED1_ON);
    os_time_dly(40);

    //兼容一些充电仓5v输出慢的时候会导致无法充电的问题
    if (get_lvcmp_det()) {
        log_info("...charge ing...\n");
        cpu_reset();
    }

    log_info("get_charge_online_flag:%d %d\n", get_charge_online_flag(), get_ldo5v_online_hw());
    if (get_ldo5v_online_hw() && get_charge_online_flag()) {
        power_set_soft_poweroff();
    } else {
#if TCFG_CHARGE_OFF_POWERON_NE
        cpu_reset();
#else
#ifdef TCFG_CHARGE_CHECK_SET_LDOIN_PINR
        charge_check_and_set_pinr(1);
#endif
        power_set_soft_poweroff();
#endif
    }
}


/*ldoin电压大于拔出电压(0.6V左右)且小于VBat电压时调用该函数进行一些错误提示或其他处理*/
void charge_err_deal(void)
{

    log_info("%s\n", __func__);

    //插入交换
    power_event_to_user(POWER_EVENT_POWER_CHANGE);

#if TCFG_GSENSOR_ENABLE
    //在舱关闭gSensor
    gSensor_wkupup_disable();
#endif

#if TCFG_LP_TOUCH_KEY_ENABLE
    extern void lp_touch_key_charge_mode_enter();
    lp_touch_key_charge_mode_enter();
#endif /* #if TCFG_LP_TOUCH_KEY_ENABLE */

#if TCFG_GX8002_NPU_ENABLE
    extern void gx8002_module_suspend(u8 keep_vddio);
    gx8002_module_suspend(0);
#endif /* #if TCFG_GX8002_NPU_ENABLE */


#ifdef TCFG_CHARGE_CHECK_SET_LDOIN_PINR
    charge_check_and_set_pinr(0);
#endif

#if TCFG_AUDIO_ANC_ENABLE
#if TCFG_ANC_BOX_ENABLE
    if (!ancbox_get_status())
#endif
    {
        anc_suspend();
    }
#endif

    if (get_charge_poweron_en() == 0) {
#if TCFG_CHARGESTORE_ENABLE
        //智能充电舱不处理充电err
        struct application *app;
        app = get_current_app();
        if (app && (!strcmp(app->name, APP_NAME_IDLE))) {
            ui_update_status(STATUS_CHARGE_LDO5V_OFF);
        }

#else //TCFG_CHARGESTORE_ENABLE

#if defined(TCFG_CHARGE_KEEP_UPDATA) && TCFG_CHARGE_KEEP_UPDATA
        if (dual_bank_update_exist_flag_get() || classic_update_task_exist_flag_get()) {
            return;
        }
#endif

#if TCFG_TEST_BOX_ENABLE || TCFG_ANC_BOX_ENABLE
        extern u8 chargestore_get_ex_enter_dut_flag(void);
        if (chargestore_get_ex_enter_dut_flag()) {
            putchar('D');
            return;
        }

        if (testbox_get_ex_enter_storage_mode_flag()) {
            putchar('S');
            return;
        }

        if (charge_err_timer == 0) {
            //延时执行避免测试盒通信不上
            charge_err_timer = sys_timeout_add(NULL, charge_err_delay_deal, 250);
        }
#else
        charge_err_delay_deal(NULL);
#endif//TCFG_TEST_BOX_ENABLE || TCFG_ANC_BOX_ENABLE
#endif//TCFG_CHARGESTORE_ENABLE
    } else {
        ui_update_status(STATUS_CHARGE_ERR);
    }
}

void charge_full_deal(void)
{
    log_info("%s\n", __func__);

    charge_full_flag = 1;

    charge_close();
    if (get_charge_poweron_en() == 0) {

#if (TCFG_LP_TOUCH_KEY_ENABLE && CHARGING_CLEAN_PHONE_INFO)
        extern void lp_touch_key_charge_mode_enter();
        lp_touch_key_charge_mode_enter();
#endif
        /* power_set_soft_poweroff(); */
#if TCFG_USER_TWS_ENABLE

#else
        ui_update_status(STATUS_CHARGE_FULL);
#endif
#if (!TCFG_CHARGESTORE_ENABLE)
        vbat_timer_delete();
#endif
    } else {
        ui_update_status(STATUS_CHARGE_FULL);
    }
}

void charge_close_deal(void)
{
    log_info("%s\n", __FUNCTION__);

    power_set_mode(TCFG_LOWPOWER_POWER_SEL);

#if TCFG_USER_TWS_ENABLE
    //在idle的时候才执行充电关闭的UI
    struct application *app;
    app = get_current_app();
    if (app && (!strcmp(app->name, APP_NAME_IDLE))) {
        ui_update_status(STATUS_CHARGE_CLOSE);
    }
#endif
}


void charge_ldo5v_in_deal(void)
{
    log_info("%s\n", __FUNCTION__);


#if TCFG_IRSENSOR_ENABLE
    if (get_bt_tws_connect_status()) {
        tws_api_sync_call_by_uuid('T', SYNC_CMD_EARPHONE_CHAREG_START, 300);
    }
#endif

    //插入交换
    power_event_to_user(POWER_EVENT_POWER_CHANGE);

    charge_full_flag = 0;

#if TCFG_GSENSOR_ENABLE
    //入舱关闭gSensor
    gSensor_wkupup_disable();
#endif

#if (TCFG_LP_TOUCH_KEY_ENABLE && !CHARGING_CLEAN_PHONE_INFO)
    extern void lp_touch_key_charge_mode_enter();
    lp_touch_key_charge_mode_enter();
#endif /* #if TCFG_LP_TOUCH_KEY_ENABLE */

#if TCFG_GX8002_NPU_ENABLE
    extern void gx8002_module_suspend(u8 keep_vddio);
    gx8002_module_suspend(0);
#endif /* #if TCFG_GX8002_NPU_ENABLE */

#ifdef TCFG_CHARGE_CHECK_SET_LDOIN_PINR
    charge_check_and_set_pinr(0);
#endif

#if TCFG_AUDIO_ANC_ENABLE
    anc_suspend();
#endif

#if TCFG_TEST_BOX_ENABLE
    chargestore_clear_testbox_status();
    if (charge_err_timer) {
        sys_timeout_del(charge_err_timer);
        charge_err_timer = 0;
    }
#endif

#if TCFG_ANC_BOX_ENABLE
    ancbox_clear_status();
    if (charge_err_timer) {
        sys_timeout_del(charge_err_timer);
        charge_err_timer = 0;
    }
#endif

#if TCFG_CHARGESTORE_ENABLE
    chargestore_shutdown_reset();
#endif

    if (get_charge_poweron_en() == 0) {
#if defined(TCFG_CHARGE_KEEP_UPDATA) && TCFG_CHARGE_KEEP_UPDATA
        if (dual_bank_update_exist_flag_get() || classic_update_task_exist_flag_get()) {
            return;
        }
#endif
        struct application *app;
        app = get_current_app();
        if (app && strcmp(app->name, APP_NAME_IDLE)) {
#if (TCFG_CHARGESTORE_ENABLE && TCFG_BLE_DEMO_SELECT == DEF_BLE_DEMO_ADV)
            if (!chargestore_check_going_to_poweroff()) {
#endif
#if TCFG_USER_TWS_ENABLE
                printf("----------------tws_api_get_tws_state %x\n", tws_api_get_tws_state());
                int deal_state = tws_api_get_tws_state();
                if (tws_api_get_role() == TWS_ROLE_MASTER &&  deal_state & TWS_STA_SIBLING_CONNECTED && deal_state & TWS_STA_PHONE_CONNECTED && !(deal_state & TWS_STA_MONITOR_START)) {
                    if (ldo5v_in_reset_timer == 0) {
                        delay_timeout = 30;
                        ldo5v_in_reset_timer = sys_timer_add(NULL, ldo5v_in_reset_delay, 100);
                    }
                } else {
                    if (ldo5v_in_reset_timer) {
                        sys_timer_del(ldo5v_in_reset_timer);
                        ldo5v_in_reset_timer = 0;
                    }
                    sys_enter_soft_poweroff((void *)1);
                }
#else
                sys_enter_soft_poweroff((void *)1);
#endif
#if (TCFG_CHARGESTORE_ENABLE && TCFG_BLE_DEMO_SELECT == DEF_BLE_DEMO_ADV)
            } else {
                log_info("chargestore do poweroff!\n");
            }
#endif
        } else {
            charge_start();
            wdt_init(WDT_32S);
            log_info("set wdt to 32s!\n");
            goto _check_reset;
        }
    } else {
        charge_start();
        goto _check_reset;
    }
    return;

_check_reset:
    //防止耳机低电时,插拔充电有几率出现关机不充电问题
    if (app_var.goto_poweroff_flag) {
        cpu_reset();
    }
}

//外置触摸供电
void __attribute__((weak)) external_touch_key_disable(void)
{

}
extern u8 chargestore_get_keep_tws_conn_flag(void);
void charge_ldo5v_off_deal(void)
{
    log_info("%s\n", __FUNCTION__);

    //拨出交换
    power_event_to_user(POWER_EVENT_POWER_CHANGE);
#if (TCFG_CHARGESTORE_ENABLE && TCFG_USER_TWS_ENABLE)
    if (TWS_ROLE_SLAVE == tws_api_get_role()) {
        chargestore_set_power_level(0xFF);
    }
#endif
    charge_full_flag = 0;

    charge_close();

    struct application *app;
    app = get_current_app();
    //在idle的时候才执行充电拔出的UI
    if (app && (!strcmp(app->name, APP_NAME_IDLE))) {
        ui_update_status(STATUS_CHARGE_LDO5V_OFF);
    }

#ifdef TCFG_CHARGE_CHECK_SET_LDOIN_PINR
    charge_check_and_set_pinr(1);
#endif

#if TCFG_TEST_BOX_ENABLE
    if (chargestore_get_testbox_status() && !get_total_connect_dev()) {
        if (!chargestore_get_keep_tws_conn_flag()) {
            log_info("<<<<<<<<<<<<<<testbox out and bt noconn reset>>>>>>>>>>>>>>>\n");
            if (chargestore_get_testbox_tws_paired() && testbox_get_softpwroff_after_paired()) {
                sys_enter_soft_poweroff(NULL);
            } else {
                cpu_reset();
            }
        } else {
            log_info("testbox out ret\n");
        }
    }
    chargestore_clear_testbox_status();
    if (charge_err_timer) {
        sys_timeout_del(charge_err_timer);
        charge_err_timer = 0;
    }
#endif

#if TCFG_CHARGESTORE_ENABLE
    chargestore_shutdown_reset();
#endif

#if TCFG_AUDIO_ANC_ENABLE
#if TCFG_ANC_BOX_ENABLE
    if (charge_err_timer) {
        sys_timeout_del(charge_err_timer);
        charge_err_timer = 0;
    }
    if (ancbox_get_status()) {
        return;
    } else
#endif
    {
        anc_resume();
    }
#endif

    if ((get_charge_poweron_en() == 0)) {

        wdt_init(WDT_4S);
        log_info("set wdt to 4s!\n");
#if TCFG_CHARGESTORE_ENABLE
        if (chargestore_get_power_status()) {
#endif
#if TCFG_CHARGE_OFF_POWERON_NE
            log_info("ldo5v off,task switch to BT\n");
            app_var.play_poweron_tone = 0;
#if TCFG_SYS_LVD_EN
            vbat_check_init();
#endif
            if (app && (app_var.goto_poweroff_flag == 0)) {
                if (strcmp(app->name, APP_NAME_BT)) {
#if TCFG_SYS_LVD_EN
                    if (get_vbat_need_shutdown() == FALSE) {
                        task_switch_to_bt();
                    } else {
                        log_info("ldo5v off,lowpower,need enter softpoweroff\n");
                        power_set_soft_poweroff();
                    }
#else
                    task_switch_to_bt();
#endif
                }
            }
#else //TCFG_CHARGE_OFF_POWERON_NE
            log_info("ldo5v off,enter softpoweroff\n");
#if TCFG_LP_TOUCH_KEY_ENABLE
            extern void lp_touch_key_charge_mode_exit();
            lp_touch_key_charge_mode_exit();
#endif /* #if TCFG_LP_TOUCH_KEY_ENABLE */

#if TCFG_TEST_BOX_ENABLE
            if (testbox_get_ex_enter_storage_mode_flag()) {
                power_set_vddio_keep(0);//关VDDIO KEEP
#if TCFG_LP_TOUCH_KEY_ENABLE
                lp_touch_key_disable();//仓储模式关内置触摸
#else
                external_touch_key_disable(); //仓储模式关外置触摸供电
#endif /* #if TCFG_LP_TOUCH_KEY_ENABLE */
            }
#endif
            power_set_soft_poweroff();
#endif //TCFG_CHARGE_OFF_POWERON_NE
#if TCFG_CHARGESTORE_ENABLE
        } else {
            log_info("ldo5v off,enter softpoweroff\n");
            if (app && (!strcmp(app->name, APP_NAME_BT))) { //软关机
                sys_enter_soft_poweroff(NULL);
            } else {
                power_set_soft_poweroff();
            }
        }
#endif
    } else {
        if (app && (!strcmp(app->name, APP_NAME_IDLE))) {
            power_set_soft_poweroff();
        }
    }
#if TCFG_GSENSOR_ENABLE
    //出舱使能gSensor
    gSensor_wkupup_enable();
#endif

#if TCFG_LP_TOUCH_KEY_ENABLE
    extern void lp_touch_key_charge_mode_exit();
    lp_touch_key_charge_mode_exit();
#endif /* #if TCFG_LP_TOUCH_KEY_ENABLE */

#if TCFG_TEST_BOX_ENABLE
    if (testbox_get_ex_enter_storage_mode_flag()) {
        power_set_vddio_keep(0);//关VDDIO KEEP
#if TCFG_LP_TOUCH_KEY_ENABLE
        lp_touch_key_disable();//仓储模式关内置触摸
#else
        external_touch_key_disable(); //仓储模式关外置触摸供电
#endif /* #if TCFG_LP_TOUCH_KEY_ENABLE */
        //测试盒仓储模式使能后，断开测试盒直接关机
        power_set_soft_poweroff();
    }
#endif

#if TCFG_GX8002_NPU_ENABLE
    extern void gx8002_module_resumed();
    gx8002_module_resumed();
#endif /* #if TCFG_GX8002_NPU_ENABLE */
}

int app_charge_event_handler(struct device_event *dev)
{
    int ret = false;
    u8 otg_status = 0;

    switch (dev->event) {
    case CHARGE_EVENT_CHARGE_START:
        charge_start_deal();
        break;
    case CHARGE_EVENT_CHARGE_CLOSE:
        charge_close_deal();
        break;
    case CHARGE_EVENT_CHARGE_ERR:
        charge_err_deal();
        break;
    case CHARGE_EVENT_CHARGE_FULL:
        charge_full_deal();
        break;
    case CHARGE_EVENT_LDO5V_IN:
#if ((TCFG_OTG_MODE & OTG_SLAVE_MODE) && (TCFG_OTG_MODE & OTG_CHARGE_MODE))
        while (1) {
            otg_status = usb_otg_online(0);
            if (otg_status != IDLE_MODE) {
                break;
            }
            os_time_dly(2);
        }
#endif
        if (get_charge_poweron_en() || (otg_status != SLAVE_MODE)) {
            charge_ldo5v_in_deal();
        }
        break;
    case CHARGE_EVENT_LDO5V_OFF:
#if ((TCFG_OTG_MODE & OTG_SLAVE_MODE) && (TCFG_OTG_MODE & OTG_CHARGE_MODE))
        while (1) {
            otg_status = usb_otg_online(0);
            if (otg_status != IDLE_MODE) {
                break;
            }
            os_time_dly(2);
        }
#endif
        if (get_charge_poweron_en() || (otg_status != SLAVE_MODE)) {
            charge_ldo5v_off_deal();
        }
        break;
    default:
        break;
    }

    return ret;
}

#else

u8 get_charge_full_flag(void)
{
    return 0;
}

#endif

