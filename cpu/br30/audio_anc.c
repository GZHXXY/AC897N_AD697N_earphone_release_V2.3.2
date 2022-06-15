/*
 ****************************************************************
 *							AUDIO ANC
 * File  : audio_anc.c
 * By    :
 * Notes : ref_mic = 参考mic
 *		   err_mic = 误差mic
 *
 ****************************************************************
 */
#include "system/includes.h"
#include "audio_anc.h"
#include "audio_anc_coeff.h"
#include "system/task.h"
#include "timer.h"
#include "asm/power/p11.h"
#include "asm/power/p33.h"
#include "online_db_deal.h"
#include "asm/anc.h"
#include "anc_btspp.h"
#include "app_config.h"
#include "tone_player.h"
#include "audio_adc.h"
#include "anc_uart.h"
#include "anc_btspp.h"
#include "audio_enc.h"
#include "app_main.h"
#include "btstack/avctp_user.h"
#if TCFG_USER_TWS_ENABLE
#include "bt_tws.h"
#endif

#if INEAR_ANC_UI
extern u8 inear_tws_ancmode;
#endif

#if TCFG_AUDIO_ANC_ENABLE

#if 1
#define user_anc_log	printf
#else
#define user_anc_log(...)
#endif


/*************************ANC增益配置******************************/
#define ANC_DAC_GAIN			7			//ANC Speaker增益
#define ANC_REF_MIC_GAIN	 	6			//ANC参考Mic增益
#define ANC_ERR_MIC_GAIN	    6			//ANC误差Mic增益
#define ANC_GAIN				-8096		//降噪模式增益
/*****************************************************************/

/*************************ANC通透配置******************************/
#define TRANSPARENCY_GAIN	    7096		//通透模式增益
/*高通和低通之间即为通透模式的带宽范围，比如低通4k（TRANS_LPF_4K）和
  高通1k（TRANS_HPF_1K）,即通透的范围是1k到4k之间 */
#define TRANSPARENCY_LPF_NUM    TRANS_LPF_4K//通透模式滤波器组选择
#define TRANSPARENCY_HPF_NUM    TRANS_HPF_1K//通透模式滤波器组选择
/*****************************************************************/
static void anc_fade_in_timeout(void *arg);
static void anc_mode_switch_deal(u8 mode);
static void anc_timer_deal(void *priv);
static int anc_app_online_parse(u8 *packet, u8 size, u8 *ext_data, u16 ext_size);

typedef struct {
    u8 state;		/*ANC状态*/
    u8 mode_num;	/*模式总数*/
    u8 mode_enable;	/*使能的模式*/
    u8 anc_parse_seq;
    u8 suspend;		/*挂起*/
    u8 ui_mode_sel; /*ui菜单降噪模式选择*/
    u16 ui_mode_sel_timer;
    u16 fade_in_timer;
    volatile u8 mode_switch_lock;
    volatile u8 sync_busy;
    audio_anc_t param;
    char mic_diff_gain;		/*动态MIC增益差值*/
    u16 mic_resume_timer;	/*动态MIC增益恢复定时器id*/
    float drc_ratio;		/*动态MIC增益，对应DRC增益比例*/
} anc_t;
static anc_t *anc_hdl = NULL;

const u8 anc_tone_tab[3] = {IDEX_TONE_ANC_OFF, IDEX_TONE_ANC_ON, IDEX_TONE_TRANSPARCNCY};
int anc_gain_tab[3] = {0, ANC_GAIN, TRANSPARENCY_GAIN};

extern u8 bt_phone_dec_is_running();
static void anc_task(void *p)
{
    int res;
    int anc_ret = 0;
    int msg[16];
    u32 pend_timeout = portMAX_DELAY;
    u8 mic_gain_temp = 0;
    user_anc_log(">>>ANC TASK<<<\n");
    while (1) {
        res = __os_taskq_pend(msg, ARRAY_SIZE(msg), pend_timeout);
        if (res == OS_TASKQ) {
            switch (msg[1]) {
            case ANC_MSG_TRAIN_OPEN:/*启动训练模式*/
                audio_mic_pwr_ctl(MIC_PWR_ON);
                user_anc_log("ANC_MSG_TRAIN_OPEN");
                audio_anc_train(&anc_hdl->param, 1);
                os_time_dly(1);
                anc_train_close();
                audio_mic_pwr_ctl(MIC_PWR_OFF);
                break;
            case ANC_MSG_TRAIN_CLOSE:/*训练关闭模式*/
                user_anc_log("ANC_MSG_TRAIN_CLOSE");
                anc_train_close();
                break;
            case ANC_MSG_RUN:
                user_anc_log("ANC_MSG_RUN:%s,ANC_GAIN:%d\n", anc_mode_str[anc_hdl->param.mode], anc_hdl->param.anc_gain);
#if TCFG_AUDIO_DYNAMIC_ADC_GAIN
                if (bt_phone_dec_is_running()) {
                    mic_gain_temp = anc_hdl->param.ref_mic_gain;
                    anc_hdl->param.ref_mic_gain = app_var.aec_mic_gain;
                }
#endif/*TCFG_AUDIO_DYNAMIC_ADC_GAIN*/
                if (anc_hdl->state == ANC_STA_INIT) {
                    audio_mic_pwr_ctl(MIC_PWR_ON);
#if ANC_MODE_SYSVDD_EN
                    clk_voltage_mode(CLOCK_MODE_USR, SYSVDD_VOL_SEL_111V);	//进入ANC时提高SYSVDD电压
#endif
                    anc_ret = audio_anc_run(&anc_hdl->param, anc_hdl->state);
                    if (anc_ret == 0) {
                        anc_hdl->state = ANC_STA_OPEN;
                    } else {
                        /*
                         *-EPERM(-1):不支持ANC(非差分MIC)
                         *-EINVAL(-22):参数错误(ANC未训练，或者anc_coeff.bin参数为空或不匹配)
                         */
                        user_anc_log("audio_anc open Failed:%d\n", anc_ret);
                        anc_hdl->mode_switch_lock = 0;
                        break;
                    }
                } else {
                    audio_anc_run(&anc_hdl->param, anc_hdl->state);
                    if (anc_hdl->param.mode == ANC_OFF) {
                        anc_hdl->state = ANC_STA_INIT;
                        /*ANC OFF && esco_dec idle*/
                        if (bt_phone_dec_is_running() == 0) {
                            audio_mic_pwr_ctl(MIC_PWR_OFF);
                        }
                    }
                }
                if (anc_hdl->param.mode == ANC_OFF) {
#if (TCFG_CLOCK_MODE == CLOCK_MODE_ADAPTIVE) && ANC_MODE_SYSVDD_EN
                    clk_voltage_mode(CLOCK_MODE_ADAPTIVE, 0);	//退出ANC恢复普通模式
#endif
                    /*anc关闭，如果没有连接蓝牙，倒计时进入自动关机*/
                    extern u8 get_total_connect_dev(void);
                    if (get_total_connect_dev() == 0) {    //已经没有设备连接
                        sys_auto_shut_down_enable();
                    }
                }
#if TCFG_AUDIO_DYNAMIC_ADC_GAIN
                if (mic_gain_temp) {
                    anc_hdl->param.ref_mic_gain = mic_gain_temp;
                    mic_gain_temp = 0;
                }
                if (bt_phone_dec_is_running() && (anc_hdl->state == ANC_STA_OPEN)) {
                    anc_dynamic_micgain_start(app_var.aec_mic_gain);
                }
#endif/*TCFG_AUDIO_DYNAMIC_ADC_GAIN*/

                anc_hdl->mode_switch_lock = 0;
                anc_hdl->fade_in_timer = usr_timeout_add((void *)0, anc_fade_in_timeout, 650, 1);
                break;
            case ANC_MSG_MODE_SYNC:
                user_anc_log("anc_mode_sync:%d", msg[2]);
                anc_mode_switch(msg[2], 0);
                break;
#if TCFG_USER_TWS_ENABLE
            case ANC_MSG_TONE_SYNC:
                user_anc_log("anc_tone_sync_play:%d", msg[2]);
                if (anc_hdl->mode_switch_lock) {
                    user_anc_log("anc mode switch lock\n");
                    break;
                }
                anc_hdl->mode_switch_lock = 1;
                if (msg[2] == SYNC_TONE_ANC_OFF) {
                    anc_hdl->param.mode = ANC_OFF;
                } else if (msg[2] == SYNC_TONE_ANC_ON) {
                    anc_hdl->param.mode = ANC_ON;
                } else {
                    anc_hdl->param.mode = ANC_TRANSPARENCY;
                }
                anc_hdl->param.anc_gain = anc_gain_tab[anc_hdl->param.mode - 1];
                if (anc_hdl->suspend) {
                    anc_hdl->param.anc_gain = 0;
                }
                tone_play_index(anc_tone_tab[anc_hdl->param.mode - 1], ANC_TONE_PREEMPTION);
                anc_mode_switch_deal(anc_hdl->param.mode);
                anc_hdl->sync_busy = 0;
                break;
#endif/*TCFG_USER_TWS_ENABLE*/
            case ANC_MSG_FADE_END:
                break;
            }
        } else {
            user_anc_log("res:%d,%d", res, msg[1]);
        }
    }
}
/*ANC训练参数初始化*/
void anc_train_para_init(anc_train_para_t *para)
{
    para->mode   		 	= ANC_TRAIN_NOMA_MODE;//默认为普通训练模式
    para->enablebit         = ANC_TRAIN_MODE;
    para->train_step   	 	= ANC_TRAIN_STEP;
    para->noise_level 	    = 3;
    para->sz_gain           = -1024;
    para->sz_lower_thr 	 	= ANC_SZ_LOW_THR;
    para->fz_lower_thr 	 	= ANC_FZ_LOW_THR;
    para->non_adaptive_time = ANC_NON_ADAPTIVE_TIME;
    para->sz_adaptive_time  = ANC_SZ_ADAPTIVE_TIME;
    para->fz_adaptive_time  = ANC_FZ_ADAPTIVE_TIME;
    para->wz_train_time 	= ANC_WZ_TRAIN_TIME;
    para->fb0sz_dly_en		= 0;
    para->fb0sz_dly_num		= 0;
    para->fb0_gain 			= 1024;
    para->mic_dma_export_en	= ANC_MIC_DMA_EXPORT;
}


void anc_param_gain_t_printf(void)
{
    printf("-------------anc_param anc_gain_t---------------\n");
    printf("dac_gain_l %d\n", anc_hdl->param.dac_gain_l);
    printf("ref_mic_gain %d\n", anc_hdl->param.ref_mic_gain);
    printf("err_mic_gain %d\n", anc_hdl->param.err_mic_gain);
    printf("fbgain %d\n", anc_hdl->param.anc_fbgain);
    printf("ffgain %d\n", anc_gain_tab[ANC_ON - 1]);
    printf("transparency_gain %d\n", anc_gain_tab[ANC_TRANSPARENCY - 1]);
    printf("anc_gain_now %d\n", anc_hdl->param.anc_gain);
    printf("trans_lpf_sel %d\n", anc_hdl->param.trans_lpf_sel);
    printf("trans_hpf_sel %d\n", anc_hdl->param.trans_hpf_sel);
    printf("filter_order %d\n", anc_hdl->param.filter_order);
    printf("sample_rate %d\n", anc_hdl->param.sample_rate);
    printf("trans_advance_mode %d\n", anc_hdl->param.trans_advance_mode);
    printf("trans_order %d\n", anc_hdl->param.trans_order);
    printf("trans_sample_rate %d\n", anc_hdl->param.trans_sample_rate);
}

/*ANC配置参数填充*/
void anc_param_fill(u8 cmd, anc_gain_t *cfg)
{
    if (cmd == ANC_CFG_READ) {
        cfg->dac_gain = anc_hdl->param.dac_gain_l;
        cfg->ref_mic_gain = anc_hdl->param.ref_mic_gain;
        cfg->err_mic_gain = anc_hdl->param.err_mic_gain;
        cfg->anc_gain = anc_gain_tab[ANC_ON - 1];
        cfg->transparency_gain = anc_gain_tab[ANC_TRANSPARENCY - 1];
        cfg->fb_gain = anc_hdl->param.anc_fbgain;
        cfg->trans_hpf_sel = anc_hdl->param.trans_hpf_sel;
        cfg->trans_lpf_sel = anc_hdl->param.trans_lpf_sel;
        cfg->order = anc_hdl->param.filter_order;
        cfg->sample_rate = anc_hdl->param.sample_rate;
        cfg->trans_advance_mode = anc_hdl->param.trans_advance_mode;
        cfg->trans_order = anc_hdl->param.trans_order;
        cfg->trans_sample_rate = anc_hdl->param.trans_sample_rate;
    } else if (cmd == ANC_CFG_WRITE) {
        anc_hdl->param.dac_gain_l = cfg->dac_gain;
        anc_hdl->param.dac_gain_r = cfg->dac_gain;
        anc_hdl->param.ref_mic_gain = cfg->ref_mic_gain;
        anc_hdl->param.err_mic_gain = cfg->err_mic_gain;
        anc_hdl->param.anc_fbgain = cfg->fb_gain;
        anc_gain_tab[ANC_ON - 1] = cfg->anc_gain;
        anc_gain_tab[ANC_TRANSPARENCY - 1] = cfg->transparency_gain;
        anc_hdl->param.trans_hpf_sel = cfg->trans_hpf_sel;
        anc_hdl->param.trans_lpf_sel = cfg->trans_lpf_sel;
        anc_hdl->param.trans_lpf_en = (anc_hdl->param.trans_lpf_sel) ? 1 : 0;
        anc_hdl->param.trans_hpf_en = (anc_hdl->param.trans_hpf_sel) ? 1 : 0;
        anc_hdl->param.ffwz_hpf_coeff = trans_filter_coeff[cfg->trans_hpf_sel];
        anc_hdl->param.ffwz_lpf_coeff = trans_filter_coeff[cfg->trans_lpf_sel];

        anc_hdl->param.filter_order = (ANC_TRAIN_MODE == ANC_HYBRID_EN && cfg->order > 2) ? 2 : cfg->order;
        anc_hdl->param.sample_rate = cfg->sample_rate;
        anc_hdl->param.trans_advance_mode = cfg->trans_advance_mode;
        anc_hdl->param.trans_order = cfg->trans_order;
        anc_hdl->param.trans_sample_rate = (anc_hdl->param.trans_advance_mode & ANC_MUTE_EN) ? ANC_TRANS_SAMPLE_RATE : cfg->sample_rate;
    }
    anc_param_gain_t_printf();
}

int anc_coeff_fill(anc_coeff_t *db_coeff)
{
    s32 *temp_coeff;
    if (db_coeff && anc_hdl->param.coeff_size) {
        anc_hdl->param.fz_coeff = db_coeff->fz_coeff;
        anc_hdl->param.coeff = db_coeff->wz_coeff;
#ifdef CONFIG_ANC_30C_ENABLE
        anc_hdl->param.sz_coeff = db_coeff->sz_coeff;
        temp_coeff = db_coeff->wz_coeff + (anc_hdl->param.wz_coeff_size >> 2);
#else
        temp_coeff = db_coeff->trans_coeff;
#endif/*CONFIG_ANC_30C_ENABLE*/
        switch (anc_hdl->param.trans_advance_mode) {
        case ANC_MUTE_EN:
            anc_hdl->param.trans_fz_coeff = temp_coeff;
            user_anc_log("ANC_TRANS_MUTE_EN\n");
            break;
        case ANC_NOISE_EN:
            anc_hdl->param.trans_coeff = temp_coeff;
            user_anc_log("ANC_TRANS_NOISE_EN\n");
            break;
        case (ANC_MUTE_EN | ANC_NOISE_EN):
            anc_hdl->param.trans_fz_coeff = temp_coeff;
            anc_hdl->param.trans_coeff = temp_coeff + 100;
            user_anc_log("ANC_TRANS_MUTE_EN & ANC_TRANS_NOISE_EN\n");
            break;
        default:
            user_anc_log("anc_fill_fail %d\n", anc_hdl->param.trans_advance_mode);
            break;
        }
#if 0
        int i = 0;
        g_printf("coeff_size %d\n", anc_hdl->param.coeff_size);
        int coeff_size_diff = 0;
#ifdef CONFIG_ANC_30C_ENABLE
        g_printf("sz coeff");
        for (i = 0; i < 100; i++) {
            printf("%d,\n", anc_hdl->param.sz_coeff[i]);
        }
        coeff_size_diff += 100;
#endif
        g_printf("fz coeff");
        for (i = 0; i < 100; i++) {
            printf("%d,\n", anc_hdl->param.fz_coeff[i]);
        }
        g_printf("wz coeff");
        for (i = 0; i < anc_hdl->param.wz_coeff_size / 4; i++) {
            printf("%d,\n", anc_hdl->param.coeff[i]);
        }
        coeff_size_diff += 100 + anc_hdl->param.wz_coeff_size / 4;
        if (anc_hdl->param.trans_advance_mode & ANC_MUTE_EN) {
            g_printf("trans fz coeff");
            for (i = 0; i < 100 ; i++) {
                printf("%d,\n", anc_hdl->param.trans_fz_coeff[i]);
            }
            coeff_size_diff += 100;
        }
        if (anc_hdl->param.trans_advance_mode & ANC_NOISE_EN) {
            g_printf("trans wz coeff");
            for (i = 0; i < anc_hdl->param.coeff_size / 4 - coeff_size_diff; i++) {
                printf("%d,\n", anc_hdl->param.trans_coeff[i]);
            }
        }
#endif
        return 0;
    }
    return 1;
}

/*ANC初始化*/
void anc_init(void)
{
    anc_hdl = zalloc(sizeof(anc_t));
    ASSERT(anc_hdl);
    anc_debug_init(&anc_hdl->param.train_para);
#if ANC_TRAIN_WAY == ANC_MANA_UART
    anc_uart_init(anc_uart_write);
#elif TCFG_ANC_TOOL_DEBUG_ONLINE
    app_online_db_register_handle(DB_PKT_TYPE_ANC, anc_app_online_parse);
#endif
    anc_hdl->mode_enable = 7;/*all_enable:0b0111*/
    anc_hdl->mode_num = 3;/*total anc mode*/
    anc_hdl->param.mode = ANC_OFF;
    anc_hdl->param.fz_fir_en = 1;
    anc_hdl->param.fz_gain = -1024;
    anc_hdl->param.train_way = ANC_TRAIN_WAY;
    anc_hdl->param.anc_fade_en = ANC_FADE_EN;/*ANC淡入淡出，默认开*/
    anc_hdl->param.anc_fade_gain = 32766;/*ANC淡入淡出增益,max:32766*/
    anc_hdl->param.dac_gain_l = ANC_DAC_GAIN;
    anc_hdl->param.dac_gain_r = ANC_DAC_GAIN;
    anc_hdl->param.ref_mic_gain = ANC_REF_MIC_GAIN;
    anc_hdl->param.err_mic_gain = ANC_ERR_MIC_GAIN;
    anc_hdl->param.sample_rate = ANC_SAMPLE_RATE;
    anc_hdl->param.filter_order = ANC_FILT_ORDER;
    anc_hdl->param.tool_enablebit = ANC_TRAIN_MODE;
    /*通透配置 start*/
    anc_hdl->param.trans_lpf_sel = TRANSPARENCY_LPF_NUM;
    anc_hdl->param.trans_hpf_sel = TRANSPARENCY_HPF_NUM;
    anc_hdl->param.trans_lpf_en = (anc_hdl->param.trans_lpf_sel) ? 1 : 0;
    anc_hdl->param.trans_hpf_en = (anc_hdl->param.trans_hpf_sel) ? 1 : 0;
    anc_hdl->param.ffwz_lpf_coeff = trans_filter_coeff[TRANSPARENCY_LPF_NUM];
    anc_hdl->param.ffwz_hpf_coeff = trans_filter_coeff[TRANSPARENCY_HPF_NUM];
    anc_hdl->param.trans_fz_gain = -256;
    anc_hdl->param.trans_sample_rate = ANC_TRANS_SAMPLE_RATE;
    anc_hdl->param.trans_order = ANC_TRANS_FILT_ORDER;
    anc_hdl->param.trans_advance_mode = ANC_TRANS_ADVANCE_MODE;
    anc_hdl->param.trans_aim_pow = ANC_TRANS_AIM_POW;
    anc_hdl->param.trans_howling_flag = 0;
    /*通透配置 end*/
#ifdef CONFIG_ANC_30C_ENABLE
    anc_hdl->param.mic_type = ANC_MIC_TYPE;
    anc_hdl->param.mic_swap_en = ANC_MIC_CH_SWAP_EN;
#endif/*CONFIG_ANC_30C_ENABLE*/
    anc_train_para_init(&anc_hdl->param.train_para);
    anc_hdl->param.coeff_size = anc_coeff_size_get();	//获取当前配置的滤波器长度

#if ANC_COEFF_SAVE_ENABLE
    anc_db_t anc_db;
    anc_db.total_size = ANC_DB_LEN_MAX;
    anc_db_init(&anc_db);
    /*读取ANC增益配置*/
    anc_gain_t *db_gain = (anc_gain_t *)anc_db_get(ANC_DB_GAIN, sizeof(anc_gain_t));
    if (db_gain) {
        user_anc_log("anc_gain_db get succ");
        anc_param_fill(ANC_CFG_WRITE, db_gain);
        anc_hdl->param.coeff_size = anc_coeff_size_get();	//获取当前配置的滤波器长度
    } else {
        user_anc_log("anc_gain_db get failed");
    }

    /*读取ANC训练系数*/
    anc_coeff_t *db_coeff = (anc_coeff_t *)anc_db_get(ANC_DB_COEFF, anc_hdl->param.coeff_size);
    if (!anc_coeff_fill(db_coeff)) {
        user_anc_log("anc_coeff_db get succ");
    } else {
        user_anc_log("anc_coeff_db empty");
#if 0
        anc_gain_t test_gain;
        test_gain.dac_gain = 8;
        test_gain.ref_mic_gain = 7;
        test_gain.err_mic_gain = 7;
        test_gain.anc_fbgain = 7095;
        test_gain.anc_gain = 7095;
        test_gain.transparency_gain = -8095;
        anc_coeff_t *test_coeff = malloc(anc_hdl->param.coeff_size);
        memcpy(test_coeff->wz_coeff, ffwz_coef_test, sizeof(test_coeff->wz_coeff));
        memset(test_coeff->fz_coeff, 0, sizeof(test_coeff->fz_coeff));
#ifdef CONFIG_ANC_30C_ENABLE
        memset(test_coeff->sz_coeff, 0, sizeof(test_coeff->sz_coeff));
        /* memset(test_coeff->trans_wz_coeff, 0, sizeof(test_coeff->trans_wz_coeff)); */
#endif/*CONFIG_ANC_30C_ENABLE*/
        int ret = anc_db_put(&anc_hdl->param, &test_gain, test_coeff);
        if (ret == 0) {
            user_anc_log("anc_db test succ\n");
        }
#endif
    }
#endif/*ANC_COEFF_SAVE_ENABLE*/

#if ANC_TRANSPARENCY_ONLY
    anc_hdl->mode_enable = 0b101;/*all_enable:0b0111*/
    anc_hdl->param.trans_advance_mode = 0;
#ifdef CONFIG_ANC_30C_ENABLE
    anc_hdl->param.sz_coeff = sz_coef_test;
#endif/*CONFIG_ANC_30C_ENABLE*/
    anc_hdl->param.coeff = ffwz_coef_test;
    anc_hdl->param.fz_coeff = fz_coef_test;
    anc_hdl->param.fz_fir_en = 0;
    anc_hdl->param.fz_gain = 0;
    anc_hdl->param.coeff_size = anc_coeff_size_get();
#endif/*ANC_TRANSPARENCY_ONLY*/

#if 0/*未训练的样机，如有需要，可以使用测试系数进行调试*/
    anc_hdl->param.coeff = ffwz_coef_test;
    anc_hdl->param.fz_coeff = fz_coef_test;
#ifdef CONFIG_ANC_30C_ENABLE
    anc_hdl->param.sz_coeff = sz_coef_test;
    /* anc_hdl->param.coeff = ffwz_30c_coef_test; */
    /* anc_hdl->param.coeff = hybrid_coef_test; */
#endif/*CONFIG_ANC_30C_ENABLE*/
    anc_hdl->param.fz_fir_en = 0;
    anc_hdl->param.fz_gain = 0;
#endif
//	sys_timer_add(NULL, anc_timer_deal, 5000);

    user_anc_log("anc_dac:%d,%d", anc_hdl->param.dac_gain_l, anc_hdl->param.dac_gain_r);
    user_anc_log("anc ref_mic:%d,err_mic:%d", anc_hdl->param.ref_mic_gain, anc_hdl->param.err_mic_gain);
    user_anc_log("anc_gain:%d,anc_fbgain:%d,trans_gain:%d", anc_gain_tab[ANC_ON - 1], anc_hdl->param.anc_fbgain, anc_gain_tab[ANC_TRANSPARENCY - 1]);

    task_create(anc_task, NULL, "anc");
    anc_hdl->state = ANC_STA_INIT;

    user_anc_log("anc_init ok");
}

void anc_train_open(u8 mode)
{
    anc_coeff_t *tmp_anc_coeff = NULL;
    anc_coeff_t *anc_coeff = NULL;
    user_anc_log("ANC_Train_Open\n");
    tmp_anc_coeff = (anc_coeff_t *)anc_db_get(ANC_DB_COEFF, anc_hdl->param.coeff_size);
    local_irq_disable();
    if (anc_hdl && (anc_hdl->state == ANC_STA_INIT)) {
        /*防止重复打开训练模式*/
        if (anc_hdl->param.mode == ANC_TRAIN || anc_hdl->param.mode == ANC_TRANS_TRAIN) {
            local_irq_enable();
            return;
        }
        /*anc工作，退出sniff*/
        user_send_cmd_prepare(USER_CTRL_ALL_SNIFF_EXIT, 0, NULL);

        /*anc工作，关闭自动关机*/
        sys_auto_shut_down_disable();
        audio_mic_pwr_ctl(MIC_PWR_ON);
        if (mode == ANC_TRAIN) {
            anc_hdl->param.anc_gain = anc_gain_tab[ANC_ON - 1];		//使用默认的增益
        } else {
            anc_hdl->param.anc_gain = anc_gain_tab[ANC_TRANSPARENCY - 1];		//使用默认的增益
        }
        anc_hdl->param.mode = mode;
        /*训练的时候，申请buf用来保存训练参数*/
        anc_coeff = malloc(anc_hdl->param.coeff_size);
        ASSERT(anc_coeff);
        if (tmp_anc_coeff) {
            memcpy(anc_coeff, tmp_anc_coeff, anc_hdl->param.coeff_size);
        }
        anc_coeff_fill(anc_coeff);
        local_irq_enable();
        anc_hdl->param.trans_howling_flag = (anc_hdl->param.trans_howling_flag) ? 1 : 0;
        os_taskq_post_msg("anc", 1, ANC_MSG_TRAIN_OPEN);
        user_anc_log("%s ok\n", __FUNCTION__);
        return;
    }
    local_irq_enable();
}

void anc_train_close(void)
{
    int ret = 0;
    if (anc_hdl == NULL) {
        return;
    }
    if (anc_hdl && (anc_hdl->param.mode == ANC_TRAIN || anc_hdl->param.mode == ANC_TRANS_TRAIN)) {
        if (anc_hdl->param.mode == ANC_TRANS_TRAIN && anc_hdl->param.trans_aim_pow) {
            anc_gain_tab[ANC_TRANSPARENCY - 1] = anc_hdl->param.anc_gain;
        }
        anc_hdl->param.mode = ANC_OFF;
        anc_hdl->state = ANC_STA_INIT;
        audio_anc_train(&anc_hdl->param, 0);
        if (anc_hdl->param.coeff) {
#if ANC_COEFF_SAVE_ENABLE
            if (!anc_hdl->param.train_err) {
                anc_gain_t tmp_anc_gain;
                anc_param_fill(ANC_CFG_READ, &tmp_anc_gain);
#ifdef CONFIG_ANC_30C_ENABLE
                ret = anc_db_put(&anc_hdl->param, &tmp_anc_gain, (anc_coeff_t *)anc_hdl->param.sz_coeff);
#else
                ret = anc_db_put(&anc_hdl->param, &tmp_anc_gain, (anc_coeff_t *)anc_hdl->param.coeff);
#endif/*CONFIG_ANC_30C_ENABLE*/
                if (ret) {
                    user_anc_log("anc_db_put err:%d\n", ret);
                }
            } else {
                user_anc_log("anc train_err %d!!!\n", anc_hdl->param.train_err);
            }
#endif/*ANC_COEFF_SAVE_ENABLE*/

#ifdef CONFIG_ANC_30C_ENABLE
            free(anc_hdl->param.sz_coeff);
            anc_hdl->param.sz_coeff = NULL;
#else
            free(anc_hdl->param.coeff);
            anc_hdl->param.coeff = NULL;
#endif/*CONFIG_ANC_30C_ENABLE*/
        }
#if ANC_COEFF_SAVE_ENABLE
        if (!anc_hdl->param.train_err) {
            anc_coeff_t *db_coeff = (anc_coeff_t *)anc_db_get(ANC_DB_COEFF, anc_hdl->param.coeff_size);
            anc_coeff_fill(db_coeff);
        }
#endif/*ANC_COEFF_SAVE_ENABLE*/
        if (anc_hdl->param.coeff) {
            user_anc_log("anc_db get succ\n");
#ifdef CONFIG_ANC_30C_ENABLE
            /* put_buf((u8 *)anc_hdl->param.sz_coeff, anc_hdl->param.coeff_size); */
#else
            /* put_buf((u8 *)anc_hdl->param, anc_hdl->param.coeff_size); */
#endif/*CONFIG_ANC_30C_ENABLE*/

        } else {
            user_anc_log("anc_db empty\n");
        }

#if TCFG_USER_TWS_ENABLE
        /*训练完毕，tws正常配置连接*/
        bt_tws_poweron();
#endif
        user_anc_log("anc_train_close ok\n");
    }
}

/*查询当前ANC是否处于训练状态*/
int anc_train_open_query(void)
{
    if (anc_hdl) {
        if (anc_hdl->param.mode == ANC_TRAIN || anc_hdl->param.mode == ANC_TRANS_TRAIN) {
            return 1;
        }
    }
    return 0;
}

extern void audio_dump();
static void anc_timer_deal(void *priv)
{
    //audio_dump();
    u8 dac_again_l = JL_ADDA->DAA_CON1 & 0xF;
    u8 dac_again_r = (JL_ADDA->DAA_CON1 >> 4) & 0xF;
    u32 dac_dgain_l = JL_AUDIO->DAC_VL0 & 0xFFFF;
    u32 dac_dgain_r = (JL_AUDIO->DAC_VL0 >> 16) & 0xFFFF;
    u8 mic0_gain = JL_ADDA->ADA_CON0 & 0x1F;
    u8 mic1_gain = (JL_ADDA->ADA_CON0 >> 5) & 0x1F;
    u32 anc_gain = JL_ANC->CON5 & 0xFFFF;
    u32 ancfb_gain = (JL_ANC->CON5 >> 16) & 0xFFFF;
    g_printf("MIC_G:%d,%d,DAC_AG:%d,%d,DAC_DG:%d,%d,ANC_G:%d ,ANC_FBG :%d\n", mic0_gain, mic1_gain, dac_again_l, dac_again_r, dac_dgain_l, dac_dgain_r, (short)anc_gain, (short)ancfb_gain);

}

static u8 last_mode = 0;
/*ANC fade*/
static void anc_fade_in_timeout(void *arg)
{
    if (anc_hdl->param.mode != ANC_OFF) {
        /* audio_anc_fz_gain(anc_hdl->param.fz_fir_en, -1024); */
    }
    last_mode = anc_hdl->param.mode;
}

static void anc_fade_out_timeout(void *arg)
{
    if (last_mode == ANC_TRANSPARENCY && anc_hdl->param.mode == ANC_ON) {
        audio_anc_gain(0, 0);	//上个模式是通透，且降噪模式不淡入淡出，先把增益写0再淡出，防止切模式啸叫
    }
    os_taskq_post_msg("anc", 1, ANC_MSG_RUN);
}

static void anc_fade(u32 gain)
{
    if (anc_hdl) {
        if (anc_hdl->param.mode != ANC_ON) {	//降噪模式不做淡入淡出，提升耳压
            user_anc_log("anc_fade:%d", gain);
            if (last_mode == ANC_TRANSPARENCY) {
                audio_anc_gain(0, 0);	//上个模式是通透，先把增益写0再淡出，防止切模式啸叫
            }
            audio_anc_fade(gain, anc_hdl->param.anc_fade_en);
        }
        if (anc_hdl->param.anc_fade_en) {
            usr_timeout_del(anc_hdl->fade_in_timer);
            /* audio_anc_fz_gain(anc_hdl->param.fz_fir_en, 0); */
            usr_timeout_add((void *)0, anc_fade_out_timeout, 800, 1);
        } else {/*不淡入淡出，则直接切模式*/
            os_taskq_post_msg("anc", 1, ANC_MSG_RUN);
        }
    }
}


/*
 *mode:降噪/通透/关闭
 *tone_play:切换模式的时候，是否播放提示音
 */
static void anc_mode_switch_deal(u8 mode)
{
    // audio_anc_gain(0, 0);
    user_anc_log("anc_state:%s", anc_state_str[anc_hdl->state]);
    if (anc_hdl->state == ANC_STA_OPEN) {
        user_anc_log("anc open now,switch mode:%d", mode);
        anc_fade(0);//切模式，先fade_out
    } else if (anc_hdl->state == ANC_STA_INIT) {
        if (anc_hdl->param.mode != ANC_OFF) {
            /*anc工作，关闭自动关机*/
            sys_auto_shut_down_disable();
            os_taskq_post_msg("anc", 1, ANC_MSG_RUN);
        } else {
            user_anc_log("anc close now,new mode is ANC_OFF\n");
            anc_hdl->mode_switch_lock = 0;
        }
    } else {
        user_anc_log("anc state err:%d\n", anc_hdl->state);
        anc_hdl->mode_switch_lock = 0;
    }
}

#define TWS_ANC_SYNC_TIMEOUT	150 //ms
void anc_mode_switch(u8 mode, u8 tone_play)
{
    if (anc_hdl == NULL) {
        return;
    }
    /*模式切换超出范围*/
    if ((mode > ANC_BYPASS) || (mode < ANC_OFF)) {
        user_anc_log("anc mode switch err:%d", mode);
        return;
    }
    /*模式切换同一个*/
    if (anc_hdl->param.mode == mode) {
        user_anc_log("anc mode switch err:same mode");
        return;
    }
    /*ANC未训练，不允许切模式*/
    /* if (anc_hdl->param.coeff == NULL) {
        user_anc_log("anc coeff NULL");
        return;
    } */
    anc_hdl->param.trans_howling_flag = (anc_hdl->param.trans_howling_flag) ? 1 : 0;
    anc_hdl->param.mode = mode;
    if (mode ==  ANC_BYPASS) {
        anc_hdl->param.anc_gain = anc_gain_tab[ANC_ON - 1] * (-1);
    } else {
        anc_hdl->param.anc_gain = anc_gain_tab[mode - 1];
    }
    if (anc_hdl->suspend) {
        anc_hdl->param.anc_gain = 0;
    }

    /*
     *ANC模式提示音播放规则
     *(1)根据应用选择是否播放提示音：tone_play
     *(2)tws连接的时候，主机发起模式提示音同步播放
     *(3)单机的时候，直接播放模式提示音
     */
    if (tone_play) {
#if TCFG_USER_TWS_ENABLE
        if (get_tws_sibling_connect_state()) {
            if (tws_api_get_role() == TWS_ROLE_MASTER) {
                user_anc_log("[tws_master]anc_tone_sync_play");
                anc_hdl->sync_busy = 1;
                if (anc_hdl->param.mode == ANC_ON) {
                    bt_tws_play_tone_at_same_time(SYNC_TONE_ANC_ON, TWS_ANC_SYNC_TIMEOUT);
                } else if (anc_hdl->param.mode == ANC_OFF) {
                    bt_tws_play_tone_at_same_time(SYNC_TONE_ANC_OFF, TWS_ANC_SYNC_TIMEOUT);
                } else {
                    bt_tws_play_tone_at_same_time(SYNC_TONE_ANC_TRANS, TWS_ANC_SYNC_TIMEOUT);
                }
            }
            return;
        } else {
            user_anc_log("anc_tone_play");
            tone_play_index(anc_tone_tab[mode - 1], ANC_TONE_PREEMPTION);
        }
#else
        tone_play_index(anc_tone_tab[mode - 1], ANC_TONE_PREEMPTION);
#endif
    }
    anc_mode_switch_deal(mode);
}

static void anc_ui_mode_sel_timer(void *priv)
{
    if (anc_hdl->ui_mode_sel && (anc_hdl->ui_mode_sel != anc_hdl->param.mode)) {
        /*
         *提示音不打断
         *tws提示音同步播放完成
         */
        if ((tone_get_status() == 0) && (anc_hdl->sync_busy == 0)) {
            user_anc_log("anc_ui_mode_sel_timer:%d,sync_busy:%d", anc_hdl->ui_mode_sel, anc_hdl->sync_busy);
            anc_mode_switch(anc_hdl->ui_mode_sel, 1);
            sys_timer_del(anc_hdl->ui_mode_sel_timer);
            anc_hdl->ui_mode_sel_timer = 0;
        }
    }
}

int anc_mode_change_tool(u8 dat)
{
    if (anc_hdl->param.train_para.enablebit != ANC_HYBRID_EN) {
        return -1;
    }
    if (dat < 0 || dat > ANC_HYBRID_EN) {
        return -1;
    }
    anc_hdl->param.tool_enablebit = dat;
    audio_anc_gain(anc_hdl->param.anc_gain, anc_hdl->param.anc_fbgain);
    return 0;
}

/*ANC通过ui菜单选择anc模式,处理快速切换的情景*/
void anc_ui_mode_sel(u8 mode, u8 tone_play)
{
    /*
     *timer存在表示上个模式还没有完成切换
     *提示音不打断
     *tws提示音同步播放完成
     */
    if ((anc_hdl->ui_mode_sel_timer == 0) && (tone_get_status() == 0) && (anc_hdl->sync_busy == 0)) {
        user_anc_log("anc_ui_mode_sel[ok]:%d", mode);
        anc_mode_switch(mode, tone_play);
    } else {
        user_anc_log("anc_ui_mode_sel[dly]:%d,timer:%d,sync_busy:%d", mode, anc_hdl->ui_mode_sel_timer, anc_hdl->sync_busy);
        anc_hdl->ui_mode_sel = mode;
        if (anc_hdl->ui_mode_sel_timer == 0) {
            anc_hdl->ui_mode_sel_timer = sys_timer_add(NULL, anc_ui_mode_sel_timer, 50);
        }
    }
}

/*tws同步播放模式提示音，并且同步进入anc模式*/
void anc_tone_sync_play(int tone_name)
{
#if TCFG_USER_TWS_ENABLE
    if (anc_hdl) {
        user_anc_log("anc_tone_sync_play:%d", tone_name);
        os_taskq_post_msg("anc", 2, ANC_MSG_TONE_SYNC, tone_name);
    }
#endif
}

/*ANC模式同步(tws模式)*/
void anc_mode_sync(u8 mode)
{
    if (anc_hdl) {
        os_taskq_post_msg("anc", 2, ANC_MSG_MODE_SYNC, mode);
    }
}

/*ANC挂起*/
void anc_suspend(void)
{
    if (anc_hdl) {
        user_anc_log("anc_suspend\n");
        anc_hdl->suspend = 1;
        audio_anc_gain(0, 0);
    }
}

/*ANC恢复*/
void anc_resume(void)
{
    if (anc_hdl) {
        user_anc_log("anc_resume\n");
        anc_hdl->suspend = 0;
        anc_hdl->param.anc_gain = anc_gain_tab[anc_hdl->param.mode - 1];
        audio_anc_gain(anc_hdl->param.anc_gain, anc_hdl->param.anc_gain);
    }
}

/*ANC信息保存*/
void anc_info_save()
{
    if (anc_hdl) {
        anc_info_t anc_info;
        int ret = syscfg_read(CFG_ANC_INFO, &anc_info, sizeof(anc_info));
        if (ret == sizeof(anc_info)) {
#if INEAR_ANC_UI
            if (anc_info.mode == anc_hdl->param.mode && anc_info.inear_tws_mode == inear_tws_ancmode) {
#else
            if (anc_info.mode == anc_hdl->param.mode) {
#endif
                user_anc_log("anc info.mode == cur_anc_mode");
                return;
            }
        } else {
            user_anc_log("read anc_info err");
        }

        user_anc_log("save anc_info");
        anc_info.mode = anc_hdl->param.mode;
#if INEAR_ANC_UI
        anc_info.inear_tws_mode = inear_tws_ancmode;
#endif
        ret = syscfg_write(CFG_ANC_INFO, &anc_info, sizeof(anc_info));
        if (ret != sizeof(anc_info)) {
            user_anc_log("anc info save err!\n");
        }

    }
}

/*系统上电的时候，根据配置决定是否进入上次的模式*/
void anc_poweron(void)
{
    if (anc_hdl) {
#if ANC_INFO_SAVE_ENABLE
        anc_info_t anc_info;
        int ret = syscfg_read(CFG_ANC_INFO, &anc_info, sizeof(anc_info));
        if (ret == sizeof(anc_info)) {
            user_anc_log("read anc_info succ,state:%s,mode:%s", anc_state_str[anc_hdl->state], anc_mode_str[anc_info.mode]);
#if INEAR_ANC_UI
            inear_tws_ancmode = anc_info.inear_tws_mode;
#endif
            if ((anc_hdl->state == ANC_STA_INIT) && (anc_info.mode != ANC_OFF)) {
                anc_mode_switch(anc_info.mode, 0);
            }
        } else {
            user_anc_log("read anc_info err");
        }
#endif
    }
}

/*ANC poweroff*/
void anc_poweroff(void)
{
    if (anc_hdl) {
        user_anc_log("anc_cur_state:%s\n", anc_state_str[anc_hdl->state]);
#if ANC_INFO_SAVE_ENABLE
        anc_info_save();
#endif/*ANC_INFO_SAVE_ENABLE*/
        if (anc_hdl->state == ANC_STA_OPEN) {
            user_anc_log("anc_poweroff\n");
            /*close anc module when fade_out timeout*/
            anc_hdl->param.mode = ANC_OFF;
            anc_fade(0);
        }
    }
}

/*模式切换测试demo*/
#define ANC_MODE_NUM	3 /*ANC模式循环切换*/
static const u8 anc_mode_switch_tab[ANC_MODE_NUM] = {
    ANC_OFF,
    ANC_TRANSPARENCY,
    ANC_ON,
};
void anc_mode_next(void)
{
    if (anc_hdl) {
        if (anc_train_open_query()) {
            return;
        }
        u8 next_mode = 0;
        local_irq_disable();
        anc_hdl->param.anc_fade_en = ANC_FADE_EN;	//防止被其他地方清0
        for (u8 i = 0; i < ANC_MODE_NUM; i++) {
            if (anc_mode_switch_tab[i] == anc_hdl->param.mode) {
                next_mode = i + 1;
                if (next_mode >= ANC_MODE_NUM) {
                    next_mode = 0;
                }
                if ((anc_hdl->mode_enable & BIT(next_mode)) == 0) {
                    user_anc_log("anc_mode_filt,next:%d,en:%d", next_mode, anc_hdl->mode_enable);
                    next_mode++;
                    if (next_mode >= ANC_MODE_NUM) {
                        next_mode = 0;
                    }
                }
                //g_printf("fine out anc mode:%d,next:%d,i:%d",anc_hdl->param.mode,next_mode,i);
                break;
            }
        }
        local_irq_enable();
        //user_anc_log("anc_next_mode:%d old:%d,new:%d", next_mode, anc_hdl->param.mode, anc_mode_switch_tab[next_mode]);
        u8 new_mode = anc_mode_switch_tab[next_mode];
        user_anc_log("new_mode:%s", anc_mode_str[new_mode]);
        anc_mode_switch(anc_mode_switch_tab[next_mode], 1);
    }
}

/*设置ANC支持切换的模式*/
void anc_mode_enable_set(u8 mode_enable)
{
    if (anc_hdl) {
        anc_hdl->mode_enable = mode_enable;
        u8 mode_cnt = 0;
        for (u8 i = 0; i < 3; i++) {
            if (mode_enable & BIT(i)) {
                mode_cnt++;
                user_anc_log("%s Select", anc_mode_str[i + 1]);
            }
        }
        user_anc_log("anc_mode_enable_set:%d", mode_cnt);
        anc_hdl->mode_num = mode_cnt;
    }
}

/*获取anc状态，0:空闲，l:忙*/
u8 anc_status_get(void)
{
    u8 status = 0;
    if (anc_hdl) {
        if (anc_hdl->state == ANC_STA_OPEN) {
            status = 1;
        }
    }
    return status;
}

/*获取anc当前模式*/
u8 anc_mode_get(void)
{
    if (anc_hdl) {
        //user_anc_log("anc_mode_get:%s", anc_mode_str[anc_hdl->param.mode]);
        return anc_hdl->param.mode;
    }
    return 0;
}
/*
 *获取anc训练状态
 *0:未训练
 *1:训练过
 */
u8 anc_train_status_get(void)
{
    if (anc_hdl && anc_hdl->param.coeff) {
        user_anc_log("anc have train coeff\n");
        return 1;
    }
    user_anc_log("anc train coeff NULL\n");
    return 0;
}

/*获取anc模式，dac左右声道的增益*/
u8 anc_dac_gain_get(u8 ch)
{
    u8 gain = 0;
    if (anc_hdl) {
        gain = (ch == ANC_DAC_CH_L) ? anc_hdl->param.dac_gain_l : anc_hdl->param.dac_gain_r;
    }
    return gain;
}

/*获取anc模式，ref_mic的增益*/
u8 anc_mic_gain_get(void)
{
    u8 gain = 0;
    if (anc_hdl) {
        gain = anc_hdl->param.ref_mic_gain;
    }
    return gain;
}

int anc_coeff_size_get(void)
{
    return anc_coeff_size_count(&anc_hdl->param);
}

/*anc coeff读接口*/
int *anc_coeff_read(void)
{
#if ANC_BOX_READ_COEFF
    int *coeff = anc_db_get(ANC_DB_COEFF, anc_hdl->param.coeff_size);
    if (coeff) {
        coeff = (int *)((u8 *)coeff);
    }
    user_anc_log("anc_coeff_read:0x%x", (u32)coeff);
    return coeff;
#else
    return NULL;
#endif/*ANC_BOX_READ_COEFF*/
}

/*anc coeff写接口*/
int anc_coeff_write(int *coeff, u16 len)
{
    int ret = 0;
    user_anc_log("anc_coeff_write:0x%x,len:%d", (u32)coeff, len);
    ret = anc_db_put(&anc_hdl->param, NULL, (anc_coeff_t *)coeff);
    if (ret) {
        user_anc_log("anc_coeff_write err:%d", ret);
        return -1;

    }
    if (anc_hdl->param.mode != ANC_OFF) {
        anc_coeff_t *db_coeff = (anc_coeff_t *)anc_db_get(ANC_DB_COEFF, anc_hdl->param.coeff_size);
        anc_coeff_fill(db_coeff);
        anc_coeff_online_update(&anc_hdl->param, 1);
    }
    return 0;
}

static u8 ANC_idle_query(void)
{
    if (anc_hdl) {
        /*ANC训练模式，不进入低功耗*/
        if (anc_train_open_query() || \
            (anc_hdl-> param.mode == ANC_TRANSPARENCY && anc_hdl->param.trans_howling_flag)) {
            return 0;
        }
    }
    return 1;
}

static enum LOW_POWER_LEVEL ANC_level_query(void)
{
    /*根据anc的状态选择sleep等级*/
    if (anc_status_get()) {
        /*anc打开，进入轻量级低功耗*/
        return LOW_POWER_MODE_LIGHT_SLEEP;
    }
    /*anc关闭，进入最优低功耗*/
    return LOW_POWER_MODE_SLEEP;
}

REGISTER_LP_TARGET(ANC_lp_target) = {
    .name       = "ANC",
    .level      = ANC_level_query,
    .is_idle    = ANC_idle_query,
};

void chargestore_uart_data_deal(u8 *data, u8 len)
{
    anc_uart_process(data, len);
}

#if TCFG_ANC_TOOL_DEBUG_ONLINE
//回复包
void anc_ci_send_packet(u32 id, u8 *packet, int size)
{
    if (DB_PKT_TYPE_ANC == id) {
        app_online_db_ack(anc_hdl->anc_parse_seq, packet, size);
        return;
    }
    /* y_printf("anc_app_spp_tx\n"); */
    ci_send_packet(id, packet, size);
}

//接收函数
static int anc_app_online_parse(u8 *packet, u8 size, u8 *ext_data, u16 ext_size)
{
    /* y_printf("anc_app_spp_rx\n"); */
    anc_hdl->anc_parse_seq  = ext_data[1];
    anc_spp_rx_packet(packet, size);
    return 0;
}
//发送函数
void anc_btspp_packet_tx(u8 *packet, int size)
{
    app_online_db_send(DB_PKT_TYPE_ANC, packet, size);
}
#endif/*TCFG_ANC_TOOL_DEBUG_ONLINE*/

u8 anc_btspp_train_again(u8 mode)
{
    /* tws_api_detach(TWS_DETACH_BY_POWEROFF); */
    if (mode == ANC_TRANS_TRAIN && !(anc_hdl->param.trans_advance_mode & ANC_MUTE_EN)) {
        user_anc_log("open ANC_TRANS_TRAIN fail!!!\n");
        return 0;	//	open fail
    }
    user_anc_log("anc_btspp_train_again\n");
    audio_anc_close();
    anc_hdl->state = ANC_STA_INIT;
    /* anc_train_close(); */
    anc_train_open(mode);
    return 1;	//	open success
}


void anc_api_set_callback(void (*callback)(u8, u8), void (*pow_callback)(anc_ack_msg_t *, u8))
{
    if (anc_hdl == NULL) {
        return;
    }
    anc_hdl->param.train_callback = callback;
    anc_hdl->param.pow_callback = pow_callback;
    user_anc_log("ANC_TRAIN_CALLBACK\n");
}

void anc_api_set_fade_en(u8 en)
{
    if (anc_hdl == NULL) {
        return;
    }
    anc_hdl->param.anc_fade_en = en;
    user_anc_log("ANC_SET_FADE_EN: %d\n", en);
}

anc_train_para_t *anc_api_get_train_param(void)
{
    if (anc_hdl == NULL) {
        return NULL;
    }
    return &anc_hdl->param.train_para;
}

u8 anc_api_get_train_step(void)
{
    if (anc_hdl == NULL) {
        return 0;
    }
    return anc_hdl->param.train_para.ret_step;
}

void anc_api_set_trans_aim_pow(u32 pow)
{
    if (anc_hdl == NULL) {
        return;
    }
    anc_hdl->param.trans_aim_pow = pow;
}

/*ANC配置在线读取接口*/
int anc_cfg_online_deal(u8 cmd, anc_gain_t *cfg)
{
    extern void audio_anc_dac_gain(u8 gain_l, u8 gain_r);
    extern void audio_anc_mic_gain(char gain0, char gain1);
    /*同步在线更新配置*/
    anc_param_fill(cmd, cfg);
    if (cmd == ANC_CFG_WRITE) {
        /*更新ANC配置*/
#if 0
        audio_anc_open(&anc_hdl->param);
#else
        if (anc_hdl->param.mode == ANC_ON) {
            anc_hdl->param.anc_gain = cfg->anc_gain;
            audio_anc_gain(cfg->anc_gain, cfg->fb_gain);
        } else if (anc_hdl->param.mode == ANC_TRANSPARENCY) {
            anc_hdl->param.anc_gain = cfg->transparency_gain;
            audio_anc_gain(cfg->transparency_gain, 0);
            anc_coeff_online_update(&anc_hdl->param, 1);
        } else if (anc_hdl->param.mode == ANC_TRAIN) {
            audio_anc_gain(cfg->anc_gain, cfg->fb_gain);
            audio_anc_dac_gain(cfg->dac_gain, cfg->dac_gain);
            audio_anc_mic_gain(cfg->ref_mic_gain, cfg->err_mic_gain);
            return 0;
        } else if (anc_hdl->param.mode == ANC_TRANS_TRAIN) {
            audio_anc_gain(cfg->transparency_gain, 0);
            audio_anc_dac_gain(cfg->dac_gain, cfg->dac_gain);
            audio_anc_mic_gain(cfg->ref_mic_gain, cfg->err_mic_gain);
            return 0;
        } else if (anc_hdl->param.mode == ANC_BYPASS) {
            anc_hdl->param.anc_gain = cfg->anc_gain * (-1);
            audio_anc_gain(anc_hdl->param.anc_gain, 0);
        }
        audio_anc_dac_gain(cfg->dac_gain, cfg->dac_gain);
        audio_anc_mic_gain(cfg->ref_mic_gain, cfg->err_mic_gain);
#endif
        /*仅修改增益配置*/
        anc_db_put(&anc_hdl->param, cfg, NULL);
    }
    return 0;
}

/*蓝牙手动训练更新增益接口*/
void anc_cfg_btspp_update(u8 id, int data)
{
    anc_gain_t cfg;
    anc_cfg_online_deal(ANC_CFG_READ, &cfg);
    switch (id) {
    case ANC_REF_MIC_GAIN_SET:
        cfg.ref_mic_gain = data;
        break;
    case ANC_ERR_MIC_GAIN_SET:
        cfg.err_mic_gain = data;
        break;
    case ANC_DAC_GAIN_SET:
        cfg.dac_gain = data;
        break;
    case ANC_FFGAIN_SET:
        cfg.anc_gain = data;
        break;
    case ANC_FBGAIN_SET:
        cfg.fb_gain = data;
        break;
    case ANC_TRANS_HPF_SET:
        cfg.trans_hpf_sel = data;
        break;
    case ANC_TRANS_LPF_SET:
        cfg.trans_lpf_sel = data;
        break;
    case ANC_TRANS_GAIN_SET:
        cfg.transparency_gain = data;
        break;
    default:
        break;
    }
    anc_cfg_online_deal(ANC_CFG_WRITE, &cfg);
}

/*蓝牙手动训练读取增益接口*/
int anc_cfg_btspp_read(u8 id, int *data)
{
    int ret = 0;
    anc_gain_t cfg;
    anc_cfg_online_deal(ANC_CFG_READ, &cfg);
    switch (id) {
    case ANC_REF_MIC_GAIN_SET:
        *data = cfg.ref_mic_gain;
        break;
    case ANC_ERR_MIC_GAIN_SET:
        *data = cfg.err_mic_gain;
        break;
    case ANC_DAC_GAIN_SET:
        *data = cfg.dac_gain;
        break;
    case ANC_FFGAIN_SET:
        *data = cfg.anc_gain;
        break;
    case ANC_FBGAIN_SET:
        *data = cfg.fb_gain;
        break;
    case ANC_SAMPLE_RATE_SET:
        *data = cfg.sample_rate;
        break;
    case ANC_ORDER_SET:
        *data = cfg.order;
        break;
    case ANC_TRANS_HPF_SET:
        *data = cfg.trans_hpf_sel;
        break;
    case ANC_TRANS_LPF_SET:
        *data = cfg.trans_lpf_sel;
        break;
    case ANC_TRANS_GAIN_SET:
        *data = cfg.transparency_gain;
        break;
    case ANC_TRANS_SAMPLE_RATE_SET:
        *data = cfg.trans_sample_rate;
        break;
    case ANC_TRANS_ORDER_SET:
        *data = cfg.trans_order;
        break;
    default:
        ret = -1;
        break;
    }
    return ret;
}

#if (ANC_MIC_TYPE&(D_MIC0|D_MIC1))
/*ANC数字MIC IO配置*/
void anc_dmic_io_init(audio_anc_t *param)
{
    if (param->mic_type & (D_MIC0 | D_MIC1)) {
        gpio_set_fun_output_port(TCFG_AUDIO_PLNK_SCLK_PIN, FO_PLNK_SCLK, 0, 1);
        gpio_set_die(TCFG_AUDIO_PLNK_SCLK_PIN, 0);
        gpio_direction_output(TCFG_AUDIO_PLNK_SCLK_PIN, 1);
        if (param->mic_type & D_MIC0) {
            //plnk data0 port init
            gpio_set_direction(TCFG_AUDIO_PLNK_DAT0_PIN, 1);
            gpio_set_die(TCFG_AUDIO_PLNK_DAT0_PIN, 1);
            gpio_set_fun_input_port(TCFG_AUDIO_PLNK_DAT0_PIN, PFI_PLNK_DAT0);
        }
        //plnk data1 port init
        if (param->mic_type & D_MIC1) {
            gpio_set_direction(TCFG_AUDIO_PLNK_DAT1_PIN, 1);
            gpio_set_die(TCFG_AUDIO_PLNK_DAT1_PIN, 1);
            gpio_set_fun_input_port(TCFG_AUDIO_PLNK_DAT1_PIN, PFI_PLNK_DAT1);
        }
    }

}
#endif/*ANC_MIC_TYPE*/

#if TCFG_AUDIO_DYNAMIC_ADC_GAIN
void anc_dynamic_micgain_start(u8 audio_mic_gain)
{
    int i;
    float ffgain;
    float multiple = 1.259;
    if (!anc_status_get()) {
        return;
    }
    anc_hdl->drc_ratio = 1.0;
    ffgain = (float)(anc_gain_tab[anc_hdl->param.mode - 1]);
    if (anc_hdl->mic_resume_timer) {
        sys_timer_del(anc_hdl->mic_resume_timer);
    }
    anc_hdl->mic_diff_gain = audio_mic_gain - anc_hdl->param.ref_mic_gain;
    if (anc_hdl->mic_diff_gain > 0) {
        for (i = 0; i < anc_hdl->mic_diff_gain; i++) {
            ffgain /= multiple;
            anc_hdl->drc_ratio *= multiple;
        }
    } else {
        for (i = 0; i > anc_hdl->mic_diff_gain; i--) {
            ffgain *= multiple;
            anc_hdl->drc_ratio /= multiple;
        }
    }
    audio_anc_gain((s16)ffgain, 0);
    user_anc_log("dynamic mic:audio_g %d,anc_g %d, diff_g %d, ff_gain %d, \n", audio_mic_gain, anc_hdl->param.ref_mic_gain, anc_hdl->mic_diff_gain, (s16)ffgain);
}

void anc_dynamic_micgain_resume_timer(void *priv)
{
    float multiple = 1.259;
    float ffgain = (float)(anc_gain_tab[anc_hdl->param.mode - 1]);
    if (anc_hdl->mic_diff_gain > 0) {
        anc_hdl->mic_diff_gain--;
        anc_hdl->drc_ratio /= multiple;
        if (!anc_hdl->mic_diff_gain) {
            anc_hdl->drc_ratio = 1.0;
        }
        ffgain /= anc_hdl->drc_ratio;
        audio_anc_gain((s16)ffgain, 0);
        audio_anc_mic_gain(anc_hdl->param.ref_mic_gain + anc_hdl->mic_diff_gain, anc_hdl->param.err_mic_gain);
        /* user_anc_log("mic0 gain %d\n", anc_hdl->param.ref_mic_gain + anc_hdl->mic_diff_gain); */
    } else if (anc_hdl->mic_diff_gain < 0) {
        anc_hdl->mic_diff_gain++;
        anc_hdl->drc_ratio *= multiple;
        if (!anc_hdl->mic_diff_gain) {
            anc_hdl->drc_ratio = 1.0;
        }
        ffgain /= anc_hdl->drc_ratio;
        audio_anc_gain((s16)ffgain, 0);
        audio_anc_mic_gain(anc_hdl->param.ref_mic_gain + anc_hdl->mic_diff_gain, anc_hdl->param.err_mic_gain);
        /* user_anc_log("mic0 gain %d\n", anc_hdl->param.ref_mic_gain + anc_hdl->mic_diff_gain); */
    } else {
        sys_timer_del(anc_hdl->mic_resume_timer);
        anc_hdl->mic_resume_timer = 0;
    }
}

void anc_dynamic_micgain_stop(void)
{
    if (anc_status_get()) {
        anc_hdl->mic_resume_timer = sys_timer_add(NULL, anc_dynamic_micgain_resume_timer, 10);
    }
}
#endif/*TCFG_AUDIO_DYNAMIC_ADC_GAIN*/

#endif/*TCFG_AUDIO_ANC_ENABLE*/
