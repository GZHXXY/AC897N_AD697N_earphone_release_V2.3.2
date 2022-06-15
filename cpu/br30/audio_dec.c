#include "asm/includes.h"
#include "media/includes.h"
#include "system/includes.h"
#include "classic/tws_api.h"
#include "classic/hci_lmp.h"
#include "effectrs_sync.h"
#include "application/audio_eq.h"
#include "application/audio_drc.h"
#include "application/audio_energy_detect.h"
#include "application/audio_surround.h"
#include "app_config.h"
#include "audio_config.h"
#include "aec_user.h"
#include "audio_enc.h"
#include "app_main.h"
#include "btstack/avctp_user.h"
#include "btstack/a2dp_media_codec.h"
#include "tone_player.h"
#include "application/audio_vbass.h"
#include "audio_iis.h"
#include "audio_syncts.h"
#include "media/bt_audio_timestamp.h"
#include "media/a2dp_sample_detect.h"
#include "audio_codec_clock.h"
#include "audio_utils.h"

#if TCFG_USER_TWS_ENABLE
#include "bt_tws.h"
#endif
#if (SYS_VOL_TYPE == VOL_TYPE_DIGITAL)
#include "audio_digital_vol.h"
#endif

#include "audio_plc.h"
#if TCFG_AUDIO_NOISE_GATE
#include "audio_noise_gate.h"
#endif/*TCFG_AUDIO_NOISE_GATE*/

#if TCFG_AUDIO_ANC_ENABLE
#include "audio_anc.h"
#endif/*TCFG_AUDIO_ANC_ENABLE*/

#if TCFG_APP_FM_EMITTER_EN
#include "fm_emitter/fm_emitter_manage.h"
#endif

#include "phone_message/phone_message.h"

#define AUDIO_CODEC_SUPPORT_SYNC	1

#define A2DP_AUDIO_PLC_ENABLE       1

#if A2DP_AUDIO_PLC_ENABLE
#include "media/tech_lib/LFaudio_plc_api.h"
#endif

#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(	".bt_decode.data.bss")
#pragma data_seg(	".bt_decode.data")
#pragma const_seg(	".bt_decode.text.const")
#pragma code_seg(	".bt_decode.text")
#endif

#define A2DP_RX_AND_AUDIO_DELAY     1

#define A2DP_EQ_SUPPORT_ASYNC		1
#define MUSIC_EQ_SUPPORT_ASYNC		1

#ifndef CONFIG_EQ_SUPPORT_ASYNC
#undef A2DP_EQ_SUPPORT_ASYNC
#define A2DP_EQ_SUPPORT_ASYNC		0
#endif

#ifndef CONFIG_EQ_SUPPORT_ASYNC
#undef MUSIC_EQ_SUPPORT_ASYNC
#define MUSIC_EQ_SUPPORT_ASYNC	0
#endif


#if A2DP_EQ_SUPPORT_ASYNC && TCFG_BT_MUSIC_EQ_ENABLE
#if TCFG_DRC_ENABLE && TCFG_BT_MUSIC_DRC_ENABLE
#define A2DP_EQ_SUPPORT_32BIT		1
#else
#define A2DP_EQ_SUPPORT_32BIT		0
#endif
#else
#define A2DP_EQ_SUPPORT_32BIT		0
#endif

#if (defined(TCFG_AUDIO_OUT_EQ_ENABLE) && (TCFG_AUDIO_OUT_EQ_ENABLE != 0))
#define AUDIO_OUT_EFFECT_ENABLE			1	// 音频输出时的音效处理
#else
#define AUDIO_OUT_EFFECT_ENABLE			0
#endif


#if AUDIO_OUT_EFFECT_ENABLE

#define AUDIO_OUT_EQ_SUPPORT_ASYNC		1

#if	((!defined(CONFIG_EQ_SUPPORT_ASYNC)))
#undef AUDIO_OUT_EQ_SUPPORT_ASYNC
#define AUDIO_OUT_EQ_SUPPORT_ASYNC		0
#endif

#if (defined(TCFG_AUDIO_OUT_EQ_ENABLE) && (TCFG_AUDIO_OUT_EQ_ENABLE != 0))
#define AUDIO_OUT_EQ_USE_SPEC_NUM		2	// 使用特定的eq段
#else
#define AUDIO_OUT_EQ_USE_SPEC_NUM		0
#endif

struct audio_out_effect_hdl {
#if (defined(TCFG_AUDIO_OUT_EQ_ENABLE) && (TCFG_AUDIO_OUT_EQ_ENABLE != 0))
    struct audio_eq *eq;
#endif
#if AUDIO_OUT_EQ_USE_SPEC_NUM
    int EQ_Coeff_table[AUDIO_OUT_EQ_USE_SPEC_NUM * 5];
#endif
#if AUDIO_OUT_EQ_SUPPORT_ASYNC
    u8 eq_async_remain;
#endif
    u8 remain;
};
struct audio_out_effect_hdl *audio_out_effect = NULL;

#endif /*AUDIO_OUT_EFFECT_ENABLE*/
static u8 audio_out_effect_dis = 0;

#define DIGITAL_OdB_VOLUME              16384

struct tone_dec_hdl {
    struct audio_decoder decoder;
    void (*handler)(void *, int argc, int *argv);
    void *priv;
};

struct a2dp_dec_hdl {
    struct audio_decoder decoder;
    struct audio_res_wait wait;
    struct audio_mixer_ch mix_ch;
    enum audio_channel channel;
    u8 start;
    u8 ch;
    s16 header_len;
    u8 remain;
    u8 eq_remain;
    u8 fetch_lock;
    u8 preempt;
    u8 stream_error;
    u8 new_frame;
    u8 repair;
    void *sample_detect;
    void *syncts;
    void *repair_pkt;
    s16 repair_pkt_len;
    u16 missed_num;
    u16 repair_frames;
    u16 overrun_seqn;
    u16 slience_frames;
#if AUDIO_CODEC_SUPPORT_SYNC
    u8 ts_start;
    u8 sync_step;
    void *ts_handle;
    u32 timestamp;
    u32 base_time;
#endif /*AUDIO_CODEC_SUPPORT_SYNC*/
#if A2DP_AUDIO_PLC_ENABLE
    LFaudio_PLC_API *plc_ops;
    void *plc_mem;
#endif /*A2DP_AUDIO_PLC_ENABLE*/
    u32 mix_ch_event_params[3];
    u16 seqn;
    u16 sample_rate;
    int timer;
    u32 coding_type;
    u16 delay_time;
    u16 detect_timer;
    u8  underrun_feedback;
#if A2DP_EQ_SUPPORT_32BIT
    s16 *eq_out_buf;
    int eq_out_buf_len;
    int eq_out_points;
    int eq_out_total;
#endif
#if (SYS_VOL_TYPE == VOL_TYPE_DIGITAL)
    dvol_handle *dvol;
#endif

#if AUDIO_SURROUND_CONFIG
    surround_hdl *surround;         //环绕音效句柄
    u8 surround_eff;  //音效模式记录
#endif

#if AUDIO_VBASS_CONFIG
    vbass_hdl *vbass;               //虚拟低音句柄
#endif
};
#if AUDIO_SURROUND_CONFIG
static u8 a2dp_surround_eff;  //音效模式记录
#endif
#define AUDIO_VBASS_TEST 0

#if AUDIO_VBASS_TEST
u16 vbass_t = 0;
extern void vbass_udate_parm_test(void *p);
#endif

struct esco_dec_hdl {
    struct audio_decoder decoder;
    struct audio_res_wait wait;
    struct audio_mixer_ch mix_ch;
    u32 coding_type;
    u8 *frame;
    u8 frame_len;
    u8 offset;
    u8 data_len;
    u8 tws_mute_en;
    u8 start;
    u8 enc_start;
    u8 frame_time;
    u8 preempt;
    u8 channels;
    u16 slience_frames;
    void *syncts;
#if AUDIO_CODEC_SUPPORT_SYNC
    void *ts_handle;
    u8 ts_start;
    u8 sync_step;
    u8 preempt_state;
#endif
    u32 mix_ch_event_params[3];
    u8 esco_len;
    u8 ul_stream_open;/*记录esco数据链路工作状态*/
    u16 sample_rate;
    u16 remain;
    int data[15];/*ALIGNED(4)*/
#if (SYS_VOL_TYPE == VOL_TYPE_DIGITAL)
    dvol_handle *dvol;
#endif
#if TCFG_ESCO_DL_NS_ENABLE
    s16 dl_ns_out[320];
    s16 dl_ns_offset;
    s16 dl_ns_remain;
#endif
    u32 hash;
    s16 fade_trigger;
    s16 fade_volume;
    s16 fade_step;

};

#if TCFG_BT_MUSIC_EQ_ENABLE
struct audio_eq *a2dp_eq = NULL;
#endif

#if TCFG_PHONE_EQ_ENABLE
struct audio_eq *esco_eq = NULL;
#endif

#if AUDIO_OUTPUT_AUTOMUTE
void *mix_out_automute_hdl = NULL;
extern void mix_out_automute_open();
extern void mix_out_automute_close();
#endif  //#if AUDIO_OUTPUT_AUTOMUTE

#if TCFG_BT_MUSIC_DRC_ENABLE
struct audio_drc *a2dp_drc = NULL;
#endif
#if (TCFG_AUDIO_BASS_BOOST && TCFG_DRC_ENABLE)
struct audio_drc *a2dp_wdrc = NULL;
#if TCFG_AUDIO_BASS_BOOST_TEST
static u8 wdrc_run = 1;
u8 test_wdrctmp = 1;
#else
static u8 wdrc_run = 1;
#endif/*TCFG_AUDIO_BASS_BOOST_TEST*/
#endif/*TCFG_AUDIO_BASS_BOOST*/
void *audio_sync = NULL;

#define MIX_POINTS_NUM  128
#if A2DP_RX_AND_AUDIO_DELAY
#define AUDIO_DAC_DELAY_TIME    30
#else
#define AUDIO_DAC_DELAY_TIME    50
#endif

#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_LR)
s16 dac_buff[4 * 1024];
#else
s16 dac_buff[2 * 1024 - 512];
#endif
#if AUDIO_CODEC_SUPPORT_SYNC
#define DEC_RUN_BY_ITSELF               0
#define DEC_PREEMTED_BY_PRIORITY        1
#define DEC_PREEMTED_BY_OUTSIDE         3
#define DEC_RESUME_BY_OUTSIDE           2
#if TCFG_BT_SUPPORT_AAC
/*AAC解码需要加大同步和MIX的buffer用来提高异步效率*/
s16 dac_sync_buff[128];
#undef MIX_POINTS_NUM
#define MIX_POINTS_NUM  128
#else
s16 dac_sync_buff[128];
#endif
s16 dac_sync_filt[24 * 2 * 2];
#endif
s16 mix_buff[MIX_POINTS_NUM];

#define MAX_SRC_NUMBER      3
s16 audio_src_hw_filt[24 * 2 * MAX_SRC_NUMBER];

#define A2DP_TO_PCM_DEVICE_FORMAT           0//48000
#define A2DP_FLUENT_STREAM_MODE             1//流畅模式
#define A2DP_FLUENT_DETECT_INTERVAL         100000//ms 流畅播放延时检测时长
#if A2DP_FLUENT_STREAM_MODE
#define A2DP_MAX_PENDING_TIME               120
#else
#define A2DP_MAX_PENDING_TIME               40
#endif

#define A2DP_STREAM_NO_ERR                  0
#define A2DP_STREAM_UNDERRUN                1
#define A2DP_STREAM_OVERRUN                 2
#define A2DP_STREAM_MISSED                  3
#define A2DP_STREAM_DECODE_ERR              4
#define A2DP_STREAM_LOW_UNDERRUN            5

/*蓝牙通话动态加载使能*/
#if TCFG_ESCO_DL_NS_ENABLE
//下行降噪占用较多ram，代码默认不动态加载
#define ESCO_DEC_MOVABLE_ENABLE		0
#else
#define ESCO_DEC_MOVABLE_ENABLE		1
#endif/*TCFG_ESCO_DL_NS_ENABLE*/
#if ESCO_DEC_MOVABLE_ENABLE
static u8 *esco_code_run_addr = NULL;
extern u8 __esco_text_cache_L2_start[];
extern u8 __esco_text_cache_L2_end[];
extern u32 __esco_movable_slot_start[];
extern u32 __esco_movable_slot_end[];
#endif/*ESCO_DEC_MOVABLE_ENABLE*/

/*OS_SEM dac_sem;*/
static u16 dac_idle_delay_max = 10000;
static u16 dac_idle_delay_cnt = 10000;
static struct tone_dec_hdl *tone_dec = NULL;
struct audio_decoder_task decode_task;
struct audio_dac_hdl dac_hdl;
struct audio_mixer mixer;
extern struct dac_platform_data dac_data;
extern struct audio_adc_hdl adc_hdl;

static u16 a2dp_delay_time;
static u8 a2dp_low_latency = 0;
static u16 drop_a2dp_timer;
static u16 a2dp_low_latency_seqn  = 0;
extern const int CONFIG_LOW_LATENCY_ENABLE;
extern const int CONFIG_A2DP_DELAY_TIME;
extern const int CONFIG_A2DP_DELAY_TIME_LO;
extern const int CONFIG_A2DP_SBC_DELAY_TIME_LO;
extern const int const_surround_en;

struct a2dp_dec_hdl *a2dp_dec = NULL;
struct esco_dec_hdl *esco_dec = NULL;

extern int pcm_decoder_enable();
extern int cvsd_decoder_init();
extern int msbc_decoder_init();
extern int g729_decoder_init();
extern int sbc_decoder_init();
extern int mp3_decoder_init();
extern int wma_decoder_init();
extern int wav_decoder_init();
extern int mty_decoder_init();
extern int flac_decoder_init();
extern int ape_decoder_init();
extern int m4a_decoder_init();
extern int amr_decoder_init();
extern int dts_decoder_init();
extern int mp3pick_decoder_init();
extern int wmapick_decoder_init();
extern int aac_decoder_init();

extern int platform_device_sbc_init();

int lmp_private_get_esco_remain_buffer_size();
int lmp_private_get_esco_data_len();
void *lmp_private_get_esco_packet(int *len, u32 *hash);
void lmp_private_free_esco_packet(void *packet);
extern int lmp_private_esco_suspend_resume(int flag);
extern int bt_audio_sync_nettime_select(u8 basetime);
extern u32 bt_audio_sync_lat_time(void);

void a2dp_decoder_wdrc_filter_setup(struct a2dp_dec_hdl *dec, struct audio_fmt *fmt);
void *a2dp_play_sync_open(u8 channel, u32 sample_rate, u32 output_rate, u32 coding_type);

void *esco_play_sync_open(u8 channel, u16 sample_rate, u16 output_rate);
//void audio_adc_init(void);
void *audio_adc_open(void *param, const char *source);
int audio_adc_sample_start(void *adc);
void audio_fade_in_fade_out(u8 left_gain, u8 right_gain);

void set_source_sample_rate(u16 sample_rate);
u8 bt_audio_is_running(void);
#if AUDIO_SURROUND_CONFIG
void surround_switch_test(void *p);
void audio_surround_voice(struct a2dp_dec_hdl *dec);
#endif
void audio_resume_all_decoder(void)
{
    audio_decoder_resume_all(&decode_task);
}

void audio_src_isr_deal(void)
{
    audio_resume_all_decoder();
}

#define AUDIO_DECODE_TASK_WAKEUP_TIME	4	//ms
#if AUDIO_DECODE_TASK_WAKEUP_TIME
#include "timer.h"
AT(.bt_decode.text.cache.L2.decoder)
static void audio_decoder_wakeup_timer(void *priv)
{
    //putchar('k');
    audio_decoder_resume_all(&decode_task);
}
int audio_decoder_task_add_probe(struct audio_decoder_task *task)
{
    local_irq_disable();
    if (task->wakeup_timer == 0) {
        task->wakeup_timer = usr_timer_add(NULL, audio_decoder_wakeup_timer, AUDIO_DECODE_TASK_WAKEUP_TIME, 1);
    }
    local_irq_enable();
    log_i("audio_decoder_task_add_probe:%d\n", task->wakeup_timer);
    return 0;
}
int audio_decoder_task_del_probe(struct audio_decoder_task *task)
{
    log_i("audio_decoder_task_del_probe\n");
    if (audio_decoder_task_wait_state(task) > 0) {
        /*解码任务列表还有任务*/
        return 0;
    }

    local_irq_disable();
    if (task->wakeup_timer) {
        log_i("audio_decoder_task_del_probe:%d\n", task->wakeup_timer);
        usr_timer_del(task->wakeup_timer);
        task->wakeup_timer = 0;
    }
    local_irq_enable();
    return 0;
}

int audio_decoder_wakeup_modify(int msecs)
{
    if (decode_task.wakeup_timer) {
        usr_timer_modify(decode_task.wakeup_timer, msecs);
    }

    return 0;
}

/*
 * BR30 : DA输出源启动后可使用DAC irq做唤醒，省去hi_timer
 */
AT(.bt_decode.text.cache.L2.decoder)
int audio_decoder_wakeup_select(u8 source)
{
    if (source == 0) {
        /*唤醒源为hi_timer*/
        local_irq_disable();
        if (!decode_task.wakeup_timer) {
            decode_task.wakeup_timer = usr_timer_add(NULL, audio_decoder_wakeup_timer, AUDIO_DECODE_TASK_WAKEUP_TIME, 1);
        }
        local_irq_enable();
    } else if (source == 1) {
#if TCFG_AUDIO_OUTPUT_IIS
        /*唤醒源为输出目标中断*/
        if (!audio_iis_is_working()) {
            return audio_decoder_wakeup_select(0);
        }

        int err = audio_iis_irq_resume_decoder(a2dp_low_latency ? 2 : AUDIO_DECODE_TASK_WAKEUP_TIME, NULL, audio_decoder_wakeup_timer);
#else
        /*唤醒源为输出目标中断*/
        if (!audio_dac_is_working(&dac_hdl)) {
            return audio_decoder_wakeup_select(0);
        }

        /*int err = audio_dac_set_irq_time(&dac_hdl, a2dp_low_latency ? 2 : AUDIO_DECODE_TASK_WAKEUP_TIME);*/
        int err = audio_dac_irq_enable(&dac_hdl, a2dp_low_latency ? 2 : AUDIO_DECODE_TASK_WAKEUP_TIME,
                                       NULL, audio_decoder_wakeup_timer);
#endif
        if (err) {
            return audio_decoder_wakeup_select(0);
        }

        local_irq_disable();
        if (decode_task.wakeup_timer) {
            usr_timer_del(decode_task.wakeup_timer);
            decode_task.wakeup_timer = 0;
        }
        local_irq_enable();
    }
    return 0;
}

#endif/*AUDIO_DECODE_TASK_WAKEUP_TIME*/

static u8 bt_dec_idle_query()
{
    if (a2dp_dec || esco_dec) {
        return 0;
    }

    return 1;
}
REGISTER_LP_TARGET(bt_dec_lp_target) = {
    .name = "bt_dec",
    .is_idle = bt_dec_idle_query,
};

AT(.audio_irq.text.cache.L1)
___interrupt
static void audio_irq_handler()
{
    /* putchar('A'); */
    if (JL_AUDIO->DAC_CON & BIT(7)) {
        JL_AUDIO->DAC_CON |= BIT(6);
        if (JL_AUDIO->DAC_CON & BIT(5)) {
            audio_decoder_resume_all(&decode_task);
            audio_dac_irq_handler(&dac_hdl);
            /*r_printf("resuem\n");*/
        }
    }

    if (JL_AUDIO->ADC_CON & BIT(7)) {
        JL_AUDIO->ADC_CON |= BIT(6);
        if ((JL_AUDIO->ADC_CON & BIT(5)) && (JL_AUDIO->ADC_CON & BIT(4))) {
            audio_adc_irq_handler(&adc_hdl);
        }
    }
}

#if A2DP_EQ_SUPPORT_32BIT
void a2dp_eq_32bit_out(struct a2dp_dec_hdl *dec)
{
    int wlen = 0;
    wlen = audio_mixer_ch_write(&dec->mix_ch, &dec->eq_out_buf[dec->eq_out_points], (dec->eq_out_total - dec->eq_out_points) * 2);
    dec->eq_out_points += wlen / 2;
}
#endif /*A2DP_EQ_SUPPORT_32BIT*/

#if TCFG_BT_MUSIC_EQ_ENABLE
AT(.bt_decode.text.cache.L2.a2dp)
static int a2dp_eq_output(void *priv, s16 *buf, u32 len)
{
    int wlen = 0;
    int rlen = len;
    s16 *data = buf;
    struct a2dp_dec_hdl *dec = priv;

#if A2DP_EQ_SUPPORT_ASYNC
    if (!dec->eq_remain) {

#if A2DP_EQ_SUPPORT_32BIT
        if (dec->eq_out_buf && (dec->eq_out_points < dec->eq_out_total)) {
            a2dp_eq_32bit_out(dec);
            if (dec->eq_out_points < dec->eq_out_total) {
                return 0;
            }
        }
#endif /*A2DP_EQ_SUPPORT_32BIT*/

#if TCFG_BT_MUSIC_DRC_ENABLE

        if (a2dp_drc) {
            audio_drc_run(a2dp_drc, data, len);
        }
#endif

#if A2DP_EQ_SUPPORT_32BIT
        if ((!dec->eq_out_buf) || (dec->eq_out_buf_len < len / 2)) {
            if (dec->eq_out_buf) {
                free(dec->eq_out_buf);
            }
            dec->eq_out_buf_len = len / 2;
            dec->eq_out_buf = malloc(dec->eq_out_buf_len);
            ASSERT(dec->eq_out_buf);
        }
        s32 *idat = data;
        s16 *odat = dec->eq_out_buf;
        for (int i = 0; i < len / 4; i++) {
            s32 outdat = *idat++;
            if (outdat > 32767) {
                outdat = 32767;
            } else if (outdat < -32768) {
                outdat = -32768;
            }
            *odat++ = outdat;
        }
        dec->eq_out_points = 0;
        dec->eq_out_total = len / 4;

        a2dp_eq_32bit_out(dec);
        return len;
#endif /*A2DP_EQ_SUPPORT_32BIT*/
    }

    /* printf("dec:0x%x, a2dp:0x%x ", dec, a2dp_dec); */
    /* printf("data:0x%x, len:%d ", data, len); */
#if 0
    do {
        /*wlen = audio_dac_write(&dac_hdl, data, len);*/
        wlen = audio_mixer_ch_write(&dec->mix_ch, data, len);
        if (!wlen) {
            break;
        }

        data += (wlen >> 1);
        len -= wlen;
    } while (len);
    return rlen - len;
#else
    return audio_mixer_ch_write(&dec->mix_ch, data, len);
#endif
#endif
    return rlen;
}
#endif


#if AUDIO_OUTPUT_AUTOMUTE

void audio_mix_out_automute_mute(u8 mute)
{
    printf(">>>>>>>>>>>>>>>>>>>> %s\n", mute ? ("MUTE") : ("UNMUTE"));
}

/* #define AUDIO_E_DET_UNMUTE      (0x00) */
/* #define AUDIO_E_DET_MUTE        (0x01) */
void mix_out_automute_handler(u8 event, u8 ch)
{
    printf(">>>> ch:%d %s\n", ch, event ? ("MUTE") : ("UNMUTE"));
    if (ch == dac_hdl.channel) {
        audio_mix_out_automute_mute(event);
    }
}

void mix_out_automute_skip(u8 skip)
{
    u8 mute = !skip;
    if (mix_out_automute_hdl) {
        audio_energy_detect_skip(mix_out_automute_hdl, 0xFFFF, skip);
        audio_mix_out_automute_mute(mute);
    }
}

void mix_out_automute_open()
{
    if (mix_out_automute_hdl) {
        printf("mix_out_automute is already open !\n");
        return;
    }
    audio_energy_detect_param e_det_param = {0};
    e_det_param.mute_energy = 5;
    e_det_param.unmute_energy = 10;
    e_det_param.mute_time_ms = 1000;
    e_det_param.unmute_time_ms = 50;
    e_det_param.count_cycle_ms = 10;
    e_det_param.sample_rate = 44100;
    e_det_param.event_handler = mix_out_automute_handler;
    e_det_param.ch_total = dac_hdl.channel;
    e_det_param.dcc = 1;
    mix_out_automute_hdl = audio_energy_detect_open(&e_det_param);
}

void mix_out_automute_close()
{
    if (mix_out_automute_hdl) {
        audio_energy_detect_close(mix_out_automute_hdl);
    }
}
#endif  //#if AUDIO_OUTPUT_AUTOMUTE

#if (defined(TCFG_AUDIO_OUT_EQ_ENABLE) && (TCFG_AUDIO_OUT_EQ_ENABLE != 0))

#if AUDIO_OUT_EQ_USE_SPEC_NUM

static struct eq_seg_info audio_out_eq_tab[AUDIO_OUT_EQ_USE_SPEC_NUM] = {
    {0, EQ_IIR_TYPE_BAND_PASS, 125,   0 << 20, (int)(0.7f * (1 << 24))},
    {1, EQ_IIR_TYPE_BAND_PASS, 12000, 0 << 20, (int)(0.3f * (1 << 24))},
};
#define AUDIO_EQ_FADE_EN  1
#if AUDIO_EQ_FADE_EN
typedef struct {
    u16 tmr;
    int cur_gain[AUDIO_OUT_EQ_USE_SPEC_NUM];
    int use_gain[AUDIO_OUT_EQ_USE_SPEC_NUM];
} _EQ_FADE_CFG;
static _EQ_FADE_CFG eq_fade_cfg = {0};
#endif





static int audio_out_eq_get_filter_info(int sr, struct audio_eq_filter_info *info)
{
    if (!audio_out_effect) {
        return -1;
    }
    local_irq_disable();
    for (int i = 0; i < AUDIO_OUT_EQ_USE_SPEC_NUM; i++) {
        eq_seg_design(&audio_out_eq_tab[i], sr, &audio_out_effect->EQ_Coeff_table[5 * i]);
    }
    local_irq_enable();

    info->L_coeff = info->R_coeff = (void *)audio_out_effect->EQ_Coeff_table;
    info->L_gain = info->R_gain = 0;
    info->nsection = AUDIO_OUT_EQ_USE_SPEC_NUM;
    return 0;
}
int audio_out_eq_spec_set_info(u8 idx, int freq, int gain)
{
    if (idx >= AUDIO_OUT_EQ_USE_SPEC_NUM) {
        return false;
    }
    if (gain > 12) {
        gain = 12;
    }
    if (gain < -12) {
        gain = -12;
    }
    if (freq) {
        audio_out_eq_tab[idx].freq = freq;
    }
    audio_out_eq_tab[idx].gain = gain << 20;

    /* printf("audio out eq, idx:%d, freq:%d,%d, gain:%d,%d \n", idx, freq, audio_out_eq_tab[idx].freq, gain, audio_out_eq_tab[idx].gain); */

#if !AUDIO_EQ_FADE_EN
    local_irq_disable();
    if (audio_out_effect && audio_out_effect->eq) {
        audio_out_effect->eq->updata = 1;
    }
    local_irq_enable();
#endif

    return true;
}

#if AUDIO_EQ_FADE_EN

int eq_fade_run(void *p)
{
    _EQ_FADE_CFG *fade_cfg = (_EQ_FADE_CFG *)p;

    u8 design = 0;
    if (fade_cfg->cur_gain[0] > fade_cfg->use_gain[0]) {
        fade_cfg->cur_gain[0] -= 1;
        if (fade_cfg->cur_gain[0] < fade_cfg->use_gain[0]) {
            fade_cfg->cur_gain[0] = fade_cfg->use_gain[0];
        }
        design = 1;
    } else if (fade_cfg->cur_gain[0] < fade_cfg->use_gain[0]) {
        fade_cfg->cur_gain[0] += 1;
        if (fade_cfg->cur_gain[0] > fade_cfg->use_gain[0]) {
            fade_cfg->cur_gain[0] = fade_cfg->use_gain[0];
        }
        design = 1;
    }

    if (design) {
        design = 0;
        audio_out_eq_spec_set_info(0, 0, fade_cfg->cur_gain[0]);//低音
        local_irq_disable();
        if (audio_out_effect && audio_out_effect->eq) {
            audio_out_effect->eq->updata = 1;
        }
        local_irq_enable();
    }

    if (fade_cfg->cur_gain[1] > fade_cfg->use_gain[1]) {
        fade_cfg->cur_gain[1] -= 1;
        if (fade_cfg->cur_gain[1] < fade_cfg->use_gain[1]) {
            fade_cfg->cur_gain[1] = fade_cfg->use_gain[1];
        }
        design = 1;
    } else if (fade_cfg->cur_gain[1] < fade_cfg->use_gain[1]) {
        fade_cfg->cur_gain[1] += 1;
        if (fade_cfg->cur_gain[1] > fade_cfg->use_gain[1]) {
            fade_cfg->cur_gain[1] = fade_cfg->use_gain[1];
        }
        design = 1;
    }

    if (design) {
        design = 0;
        audio_out_eq_spec_set_info(1, 0, fade_cfg->cur_gain[1]);//高音

        local_irq_disable();
        if (audio_out_effect && audio_out_effect->eq) {
            audio_out_effect->eq->updata = 1;
        }
        local_irq_enable();
    }

    return 0;
}



#endif


int audio_out_eq_spec_set_gain(u8 idx, int gain)
{
    if (!idx) {
        idx = 0;
    } else {
        idx = 1;
    }

#if AUDIO_EQ_FADE_EN
    eq_fade_cfg.use_gain[idx] = gain;
    if (eq_fade_cfg.tmr == 0) {
        eq_fade_cfg.tmr = sys_hi_timer_add(&eq_fade_cfg, eq_fade_run, 20);
    }
#else
    if (!idx) {
        audio_out_eq_spec_set_info(0, 0, gain);//低音
    } else {
        audio_out_eq_spec_set_info(1, 0, gain);//高音
    }
#endif

    return true;
}

#endif /*AUDIO_OUT_EQ_USE_SPEC_NUM*/


static int mix_output_handler(struct audio_mixer *mixer, s16 *data, u16 len);
static int audio_out_eq_output(void *priv, s16 *data, u32 len)
{
#if AUDIO_OUT_EQ_SUPPORT_ASYNC
    if (!audio_out_effect->eq_async_remain) {
    }
    int wlen = mix_output_handler(priv, data, len);
    if (wlen == len) {
        audio_out_effect->eq_async_remain = 0;
    } else {
        audio_out_effect->eq_async_remain = 1;
    }
    return wlen;
#endif
    return len;
}


static void audio_out_effect_close(void)
{
    if (!audio_out_effect) {
        return ;
    }
#if (defined(TCFG_AUDIO_OUT_EQ_ENABLE) && (TCFG_AUDIO_OUT_EQ_ENABLE != 0))

#if AUDIO_EQ_FADE_EN
    if (eq_fade_cfg.tmr) {
        sys_hi_timer_del(eq_fade_cfg.tmr);
        eq_fade_cfg.tmr = 0;
    }
#endif
    if (audio_out_effect->eq) {
        audio_eq_close(audio_out_effect->eq);
        local_irq_disable();
        free(audio_out_effect->eq);
        audio_out_effect->eq = NULL;
        local_irq_enable();
    }
#endif
    local_irq_disable();
    free(audio_out_effect);
    audio_out_effect = NULL;
    local_irq_enable();
}

static int audio_out_effect_stream_clear()
{
    if (!audio_out_effect) {
        return 0;
    }

    if (audio_out_effect->eq) {
#if AUDIO_OUT_EQ_SUPPORT_ASYNC
        audio_eq_async_data_clear(audio_out_effect->eq);
#endif
    }

    return 0;
}

static int audio_out_effect_open(void *priv, u16 sample_rate, u8 ch_num)
{
    audio_out_effect_close();

    struct audio_out_effect_hdl *hdl = zalloc(sizeof(struct audio_out_effect_hdl));
    if (!hdl) {
        return false;
    }
#if (defined(TCFG_AUDIO_OUT_EQ_ENABLE) && (TCFG_AUDIO_OUT_EQ_ENABLE != 0))
    hdl->eq = zalloc(sizeof(struct audio_eq) + sizeof(struct hw_eq_ch));
    if (hdl->eq) {
        hdl->eq->eq_ch = (struct hw_eq_ch *)((int)hdl->eq + sizeof(struct audio_eq));
        struct audio_eq_param eq_param = {0};
        eq_param.channels = ch_num;
#if AUDIO_OUT_EQ_USE_SPEC_NUM
        eq_param.max_nsection = AUDIO_OUT_EQ_USE_SPEC_NUM;
        eq_param.cb = audio_out_eq_get_filter_info;
#else
        eq_param.online_en = 1;
        eq_param.mode_en = 1;
        eq_param.remain_en = 1;
        eq_param.max_nsection = EQ_SECTION_MAX;
        eq_param.cb = eq_get_filter_info;
#endif

#if AUDIO_OUT_EQ_SUPPORT_ASYNC
        eq_param.no_wait = 1;//异步
#endif
        eq_param.eq_name = 0;
        audio_eq_open(hdl->eq, &eq_param);
        audio_eq_set_samplerate(hdl->eq, sample_rate);
        audio_eq_set_output_handle(hdl->eq, audio_out_eq_output, priv);
        audio_eq_start(hdl->eq);
    }
#endif
    audio_out_effect = hdl;
    return true;
}


static int mix_output_effect_handler(struct audio_mixer *mixer, s16 *data, u16 len)
{
    if (!audio_out_effect) {
        return mix_output_handler(mixer, data, len);
    }
#if AUDIO_OUT_EQ_SUPPORT_ASYNC
    if (audio_out_effect->eq) {
        return audio_eq_run(audio_out_effect->eq, data, len);
    }
#endif /*AUDIO_OUT_EQ_SUPPORT_ASYNC*/


    if (!audio_out_effect->remain) {
#if (defined(TCFG_AUDIO_OUT_EQ_ENABLE) && (TCFG_AUDIO_OUT_EQ_ENABLE != 0))
        if (audio_out_effect->eq) {
            audio_eq_run(audio_out_effect->eq, data, len);
        }
#endif
    }
    int wlen = mix_output_handler(mixer, data, len);
    if (wlen == len) {
        audio_out_effect->remain = 0;
    } else {
        audio_out_effect->remain = 1;
    }
    return wlen;
}
#endif

extern void audio_adc_mic_demo(u8 mic_idx, u8 gain, u8 mic_2_dac);
extern void dac_analog_power_control(u8 en);
extern void audio_dac_output_enable(struct audio_dac_hdl *dac);
extern void audio_dac_output_disable(struct audio_dac_hdl *dac);

u8 audio_fast_mode = 0;
void audio_fast_mode_test()
{
    audio_fast_mode = 1;
    audio_dac_set_volume(&dac_hdl, app_audio_get_volume(APP_AUDIO_CURRENT_STATE));
    dac_analog_power_control(1);////将关闭基带，不开可发现，不可连接
    audio_dac_start(&dac_hdl);
    audio_adc_mic_demo(AUDIO_ADC_MIC_CH, 10, 1);

}
u8 dac_power_off_flag = 1;
u8 is_dac_power_off()
{
    if (a2dp_dec || esco_dec || get_call_status() != BT_CALL_HANGUP) {
        return 1;
    }
    return dac_power_off_flag;
}

void dac_analog_power_control(u8 en)
{
#if 0
    if (en) {
        dac_power_off_flag = 0;
        audio_dac_output_enable(&dac_hdl);
    } else {
#if TCFG_AUDIO_ANC_ENABLE
        if (!anc_status_get()) {
            puts("anc dac_power_off");
            audio_dac_output_disable(&dac_hdl);
            /* dac_power_off(); */
        }
#else
        puts("dac_power_off");
        /* dac_power_off(); */
        audio_dac_output_disable(&dac_hdl);

#endif
        dac_power_off_flag = 1;
    }
#endif
}


static void mixer_event_handler(struct audio_mixer *mixer, int event)
{
    switch (event) {
    case MIXER_EVENT_CH_OPEN:
        printf("** MIXER_EVENT_CH_OPEN **\n");
        if (audio_mixer_get_start_ch_num(mixer) == 1) {
#if AUDIO_OUTPUT_AUTOMUTE
            audio_energy_detect_sample_rate_update(mix_out_automute_hdl, audio_mixer_get_sample_rate(mixer));
#endif  //#if AUDIO_OUTPUT_AUTOMUTE

#if TCFG_AUDIO_OUTPUT_IIS
            audio_iis_output_set_srI(audio_mixer_get_sample_rate(mixer));
            audio_iis_output_start(1);
#elif TCFG_APP_FM_EMITTER_EN

#else
            if (A2DP_TO_PCM_DEVICE_FORMAT && a2dp_dec && a2dp_dec->start) {
                audio_dac_set_sample_rate(&dac_hdl, A2DP_TO_PCM_DEVICE_FORMAT);
            } else {
                audio_dac_set_sample_rate(&dac_hdl, audio_mixer_get_sample_rate(mixer));
            }
            audio_dac_set_volume(&dac_hdl, app_audio_get_volume(APP_AUDIO_CURRENT_STATE));
            audio_dac_start(&dac_hdl);
            dac_analog_power_control(1);
            g_printf("MIXER_EVENT_CH_OPEN=%d\n", dac_power_off_flag);
            audio_dac_set_input_correct_callback(&dac_hdl,
                                                 (void *)mixer,
                                                 (void (*)(void *, int))audio_mixer_position_correct);
#endif
#if AUDIO_OUT_EFFECT_ENABLE
#if TCFG_APP_FM_EMITTER_EN
            u8 ch_num = 2;
#else
            u8 ch_num = 1;
            int dac_output = audio_dac_get_channel(&dac_hdl);
            if (dac_output == DAC_OUTPUT_LR) {
                ch_num = 2;
            }
#endif

            if (!audio_out_effect_dis) {
                audio_out_effect_open(mixer, audio_mixer_get_sample_rate(mixer), ch_num);
            }
#endif

        }
        break;
    case MIXER_EVENT_CH_CLOSE:
        if (audio_mixer_get_ch_num(mixer) == 0) {
            audio_dac_set_input_correct_callback(&dac_hdl, NULL, NULL);
#if AUDIO_OUT_EFFECT_ENABLE
            audio_out_effect_close();
#endif

#if TCFG_AUDIO_OUTPUT_IIS
            audio_iis_output_stop();
#elif TCFG_APP_FM_EMITTER_EN

#else
            audio_dac_set_protect_time(&dac_hdl, 0, NULL, NULL);
            audio_dac_stop(&dac_hdl);
            dac_analog_power_control(0);
            g_printf("MIXER_EVENT_CH_CLOSE=%d\n", dac_power_off_flag);
            if (audio_fast_mode) {
                audio_fast_mode_test();
            }
#endif
        }
        break;
    }
}

static int mix_probe_handler(struct audio_mixer *mixer)
{
    return 0;
}

#define IIS_MONO_TO_DUAL_POINTS  64
static inline void audio_pcm_mono_to_dual(s16 *dual_pcm, s16 *mono_pcm, int points);
AT(.bt_decode.text.cache.L2.mixer)
static int mix_output_handler(struct audio_mixer *mixer, s16 *data, u16 len)
{
    int rlen = len;
    int wlen = 0;

    /*os_sem_set(&dac_sem, 0);*/
    /* audio_aec_refbuf(data, len); */

#if AUDIO_OUTPUT_AUTOMUTE
    audio_energy_detect_run(mix_out_automute_hdl, data, len);
#endif  //#if AUDIO_OUTPUT_AUTOMUTE

#if TCFG_AUDIO_OUTPUT_IIS
    u8 dac_connect_mode = audio_dac_get_channel(&dac_hdl);
    if (dac_connect_mode == DAC_OUTPUT_LR) {
        wlen = audio_iis_output_write(data, len);
    } else {
        s16 *mono_data = (s16 *)data;
        s16 point_num = 0;
        u16 remain_points = (len >> 1);
        s16 two_ch_data[IIS_MONO_TO_DUAL_POINTS * 2];

        do {
            point_num = IIS_MONO_TO_DUAL_POINTS;
            if (point_num >= remain_points) {
                point_num = remain_points;
            }
            audio_pcm_mono_to_dual(two_ch_data, mono_data, point_num);
            int tmp_len = audio_iis_output_write(two_ch_data, point_num << 2);
            wlen += tmp_len;
            remain_points -= (tmp_len >> 2);
            if (tmp_len < (point_num << 2)) {
                break;
            }
            mono_data += point_num;
        } while (remain_points);
        wlen = wlen >> 1;
    }

#elif TCFG_APP_FM_EMITTER_EN
    fm_emitter_cbuf_write((u8 *)data, len);
#else
    wlen = audio_dac_write(&dac_hdl, data, len);
#endif

    return wlen;
}

static const struct audio_mix_handler mix_handler  = {
    .mix_probe  = mix_probe_handler,

#if AUDIO_OUT_EFFECT_ENABLE
    .mix_output = mix_output_effect_handler,
#else
    .mix_output = mix_output_handler,
#endif
};


void audio_mix_ch_event_handler(void *priv, int event)
{
    switch (event) {
    case MIXER_EVENT_CH_OPEN:
        if (priv) {
            u32 *params = (u32 *)priv;
            struct audio_mixer_ch *ch = (struct audio_mixer_ch *)params[0];
            u32 base_time = params[2];
#if defined(TCFG_AUDIO_OUTPUT_IIS) && TCFG_AUDIO_OUTPUT_IIS
            audio_iis_add_syncts_handle((void *)params[1]);
#else
            audio_dac_add_syncts_handle(&dac_hdl, (void *)params[1]);
#endif
            u32 current_time = (bt_audio_sync_lat_time() * 625 * TIME_US_FACTOR);
            u32 time_diff = ((base_time - current_time) & 0xffffffff) / TIME_US_FACTOR;
            /*printf("-----base time : %u, current_time : %u------\n", base_time, current_time);*/
            if (time_diff < 500000) {
                int buf_frame = audio_dac_buffered_frames(&dac_hdl);
#if defined(TCFG_AUDIO_OUTPUT_IIS) && TCFG_AUDIO_OUTPUT_IIS
                buf_frame = audio_iis_buffered_frames();
#endif
                int slience_frames = (u64)time_diff * audio_mixer_get_sample_rate(&mixer) / 1000000 - buf_frame;
                if (slience_frames < 0) {
                    slience_frames = 0;
                }
                /*printf("-------slience_frames : %d-------\n", slience_frames);*/
                sound_pcm_update_frame_num((void *)params[1], -slience_frames);
                audio_mixer_ch_add_slience_samples(ch, slience_frames * dac_hdl.channel);
            }
        }
        break;
    case MIXER_EVENT_CH_CLOSE:
        if (priv) {
            u32 *params = (u32 *)priv;
#if defined(TCFG_AUDIO_OUTPUT_IIS) && TCFG_AUDIO_OUTPUT_IIS
            audio_iis_remove_syncts_handle((void *)params[1]);
#else
            audio_dac_remove_syncts_handle(&dac_hdl, (void *)params[1]);
#endif
        }
        break;
    }
}

#define RB16(b)    (u16)(((u8 *)b)[0] << 8 | (((u8 *)b))[1])
#define RB32(b)    (u32)(((u8 *)b)[0] << 24 | (((u8 *)b))[1] << 16 | (((u8 *)b))[2] << 8 | (((u8 *)b))[3])

AT(.bt_decode.text.cache.L2.a2dp)
static int get_rtp_header_len(u8 new_frame, u8 *buf, int len)
{
    int ext, csrc;
    int byte_len;
    int header_len = 0;
    u8 *data = buf;

    csrc = buf[0] & 0x0f;
    ext  = buf[0] & 0x10;

    byte_len = 12 + 4 * csrc;
    buf += byte_len;

    if (ext) {
        ext = (RB16(buf + 2) + 1) << 2;
    }

    if (new_frame) {
        header_len = byte_len + ext + 1;
    } else {
        header_len = byte_len + ext;
    }
    if (a2dp_dec->coding_type == AUDIO_CODING_SBC) {
        while (data[header_len] != 0x9c) {
            if (++header_len > len) {
                return len;
            }
        }
    }

    return header_len;
}

__attribute__((weak))
int audio_dac_get_channel(struct audio_dac_hdl *p)
{
    return 0;
}

void __a2dp_drop_frame(void *p)
{
    int len;
    u8 *frame;

#if 0
    int num = a2dp_media_get_packet_num();
    if (num > 1) {
        for (int i = 0; i < num; i++) {
            len = a2dp_media_get_packet(&frame);
            if (len <= 0) {
                break;
            }
            //printf("a2dp_drop_frame: %d\n", len);
            a2dp_media_free_packet(frame);
        }
    }
#else
    while (1) {
        len = a2dp_media_try_get_packet(&frame);
        if (len <= 0) {
            break;
        }
        a2dp_media_free_packet(frame);
    }

#endif

}

static void __a2dp_clean_frame_by_number(struct a2dp_dec_hdl *dec, u16 num)
{
    u16 end_seqn = dec->seqn + num;
    if (end_seqn == 0) {
        end_seqn++;
    }
    /*__a2dp_drop_frame(NULL);*/
    /*dec->drop_seqn = end_seqn;*/
    a2dp_media_clear_packet_before_seqn(end_seqn);
}

static void a2dp_tws_clean_frame(void *arg)
{
    u8 master = 0;
#if TCFG_USER_TWS_ENABLE
    if (tws_api_get_role() == TWS_ROLE_MASTER) {
        master = 1;
    }
#else
    master = 1;
#endif
    if (!master) {
        return;
    }

    int msecs = a2dp_media_get_remain_play_time(0);
    if (msecs <= 0) {
        return;
    }

    if (a2dp_dec && a2dp_dec->fetch_lock) {
        return;
    }
    int len = 0;
    u16 seqn = 0;
    u8 *packet = a2dp_media_fetch_packet(&len, NULL);
    if (!packet) {
        return;
    }
    seqn = RB16(packet + 2) + 10;
    if (seqn == 0) {
        seqn = 1;
    }
    a2dp_media_clear_packet_before_seqn(seqn);
}

static u8 a2dp_suspend = 0;
static u32 a2dp_resume_time = 0;


int a2dp_decoder_pause(void)
{
    if (a2dp_dec) {
        return audio_decoder_pause(&(a2dp_dec->decoder));
    }

    return 0;
}

int a2dp_decoder_start(void)
{
    if (a2dp_dec) {
        return audio_decoder_start(&(a2dp_dec->decoder));
    }

    return 0;
}
u8 get_a2dp_drop_frame_flag()
{
    if (a2dp_dec) {
        return a2dp_dec->timer;
    }
    return 0;
}

void a2dp_drop_frame_start()
{
    if (a2dp_dec && (a2dp_dec->timer == 0)) {
        a2dp_dec->timer = sys_timer_add(NULL, __a2dp_drop_frame, 50);
        a2dp_tws_audio_conn_offline();
    }
}

void a2dp_drop_frame_stop()
{
    if (a2dp_dec && a2dp_dec->timer) {
        sys_timer_del(a2dp_dec->timer);
        a2dp_tws_audio_conn_delete();
        a2dp_dec->timer = 0;
    }
}

AT(.bt_decode.text.cache.L2.a2dp)
static void a2dp_dec_set_output_channel(struct a2dp_dec_hdl *dec)
{
    int state = 0;
    enum audio_channel channel;
    u8 dac_connect_mode = 0;

#if TCFG_USER_TWS_ENABLE
    state = tws_api_get_tws_state();
    if (state & TWS_STA_SIBLING_CONNECTED) {
        channel = tws_api_get_local_channel() == 'L' ? AUDIO_CH_L : AUDIO_CH_R;
        dec->ch = 1;
    } else {
        dac_connect_mode = audio_dac_get_channel(&dac_hdl);
        if (dac_connect_mode == DAC_OUTPUT_LR) {
            channel = AUDIO_CH_LR;
            dec->ch = 2;
        } else {
            channel = AUDIO_CH_DIFF;
            dec->ch = 1;
        }
    }

#else
    dac_connect_mode = audio_dac_get_channel(&dac_hdl);
    if (dac_connect_mode == DAC_OUTPUT_LR) {
        channel = AUDIO_CH_LR;
        dec->ch = 2;
    } else {
        channel = AUDIO_CH_DIFF;
        dec->ch = 1;
    }
#endif

#if TCFG_APP_FM_EMITTER_EN
    channel = AUDIO_CH_LR;
#endif
    if (channel != dec->channel) {
        printf("set_channel: %d\n", channel);
        audio_decoder_set_output_channel(&dec->decoder, channel);
        dec->channel = channel;
#if TCFG_BT_MUSIC_EQ_ENABLE
        if (a2dp_eq) {
            audio_eq_set_channel(a2dp_eq, dec->ch);
        }
#endif

#if TCFG_USER_TWS_ENABLE
#if AUDIO_SURROUND_CONFIG
        u8 nch = EFFECT_CH_L;
        if (channel == AUDIO_CH_L) {
            nch = EFFECT_CH_L;
        } else if (channel == AUDIO_CH_R) {
            nch = EFFECT_CH_R;
        } else if (channel == AUDIO_CH_LR) {
            nch = 2;
        }

        if (dec->surround) {
            audio_surround_switch_nch(dec->surround, nch);
        }
#endif
#endif
    }
}

/*
 *
 */
AT(.bt_decode.text.cache.L2.a2dp)
static int a2dp_decoder_set_timestamp(struct a2dp_dec_hdl *dec, u16 seqn)
{
#if AUDIO_CODEC_SUPPORT_SYNC
    u32 timestamp;

    timestamp = a2dp_audio_update_timestamp(dec->ts_handle, seqn, audio_syncts_get_dts(dec->syncts));
    if (!dec->ts_start) {
        dec->ts_start = 1;
        dec->mix_ch_event_params[2] = timestamp;
    } else {
        audio_syncts_next_pts(dec->syncts, timestamp);
        audio_syncts_update_sample_rate(dec->syncts, a2dp_audio_sample_rate(dec->ts_handle));;
    }
    dec->timestamp = timestamp;
    /*printf("timestamp : %d, %d\n", seqn, timestamp / TIME_US_FACTOR / 625);*/
#endif
    return 0;
}

static bool a2dp_audio_is_underrun(struct a2dp_dec_hdl *dec)
{
#if AUDIO_CODEC_SUPPORT_SYNC
    if (dec->ts_start != 2) {
        return false;
    }
#endif
    int underrun_time = a2dp_low_latency ? 1 : 20;
#if defined(TCFG_AUDIO_OUTPUT_IIS) && TCFG_AUDIO_OUTPUT_IIS
    if (audio_iis_data_time() < underrun_time) {
        return true;
    }
#else
    if (audio_dac_data_time(&dac_hdl) < underrun_time) {
        return true;
    }
#endif
    return false;
}

static bool a2dp_bt_rx_overrun(void)
{
    return a2dp_media_get_remain_buffer_size() < 768 ? true : false;
}

static void a2dp_decoder_stream_free(struct a2dp_dec_hdl *dec, void *packet)
{
    if (packet) {
        a2dp_media_free_packet(packet);
    }

    if ((void *)packet == (void *)dec->repair_pkt) {
        dec->repair_pkt = NULL;
    }

    if (dec->repair_pkt) {
        a2dp_media_free_packet(dec->repair_pkt);
        dec->repair_pkt = NULL;
    }
    dec->repair_pkt_len = 0;
}

static void a2dp_stream_underrun_feedback(void *priv);
static int a2dp_audio_delay_time(struct a2dp_dec_hdl *dec);
#define a2dp_seqn_before(a, b)  ((a < b && (u16)(b - a) < 1000) || (a > b && (u16)(a - b) > 1000))
static int a2dp_buffered_stream_sample_rate(struct a2dp_dec_hdl *dec, u8 *from_packet, u16 *end_seqn)
{
    u8 *packet = from_packet;
    int len = 0;
    int sample_rate = 0;

    if (!dec->sample_detect) {
        return dec->sample_rate;
    }

    a2dp_frame_sample_detect_start(dec->sample_detect, a2dp_media_dump_rx_time(packet));
    while (1) {
        packet = a2dp_media_fetch_packet(&len, packet);
        if (!packet) {
            break;
        }
        sample_rate = a2dp_frame_sample_detect(dec->sample_detect, packet, len, a2dp_media_dump_rx_time(packet));
        *end_seqn = RB16(packet + 2);
    }

    /*printf("A2DP sample detect : %d - %d\n", sample_rate, dec->sample_rate);*/
    return sample_rate;
}

static int a2dp_stream_overrun_handler(struct a2dp_dec_hdl *dec, u8 **frame, int *len)
{
    u8 *packet = NULL;
    int rlen = 0;

    int msecs = 0;
    int overrun = 0;
    int sample_rate = 0;
    u16 from_seqn = RB16(dec->repair_pkt + 2);
    u16 seqn = from_seqn;
    u16 end_seqn = 0;
    overrun = 1;
    while (1) {
        msecs = a2dp_audio_delay_time(dec);
        if (msecs < (CONFIG_A2DP_DELAY_TIME + 50) && !a2dp_bt_rx_overrun()) {
            break;
        }
        overrun = 0;
        rlen = a2dp_media_try_get_packet(&packet);
        if (rlen <= 0) {
            break;
        }

        sample_rate = a2dp_buffered_stream_sample_rate(dec, dec->repair_pkt, &end_seqn);
        a2dp_decoder_stream_free(dec, NULL);
        dec->repair_pkt = packet;
        dec->repair_pkt_len = rlen;
        seqn = RB16(packet + 2);
        if (!a2dp_seqn_before(seqn, dec->overrun_seqn)) {
            *frame = packet;
            *len = rlen;
            /*printf("------> end frame : %d\n", dec->overrun_seqn);*/
            return 1;
        }

        if (/*sample_rate < (dec->sample_rate * 4 / 5) || */sample_rate > (dec->sample_rate * 4 / 3)) {
            if (a2dp_seqn_before(dec->overrun_seqn, end_seqn)) {
                dec->overrun_seqn = end_seqn;
            }
            continue;
        }
    }
    if (overrun) {
        /*putchar('+');*/
        /* dec->overrun_seqn++; */
    } else {
        /*putchar('-');*/
    }

    *frame = dec->repair_pkt;
    *len = dec->repair_pkt_len;
    return 0;
}

static int a2dp_stream_missed_handler(struct a2dp_dec_hdl *dec, u8 **frame, int *len)
{
    int msecs = a2dp_audio_delay_time(dec);
    *frame = dec->repair_pkt;
    *len = dec->repair_pkt_len;
    if ((msecs >= (dec->delay_time + 50) || a2dp_bt_rx_overrun()) || --dec->missed_num == 0) {
        /*putchar('M');*/
        return 1;
    }
    /*putchar('m');*/
    return 0;
}

static int a2dp_stream_underrun_handler(struct a2dp_dec_hdl *dec, u8 **packet)
{

    if (!a2dp_audio_is_underrun(dec)) {
        putchar('x');
        return 0;
    }
    putchar('X');
    if (dec->stream_error != A2DP_STREAM_UNDERRUN) {
        if (!dec->stream_error) {
            a2dp_stream_underrun_feedback(dec);
        }
        dec->stream_error = a2dp_low_latency ? A2DP_STREAM_LOW_UNDERRUN : A2DP_STREAM_UNDERRUN;
        dec->repair = a2dp_low_latency ? 0 : 1;
    }
    *packet = dec->repair_pkt;
    dec->repair_frames++;
    return dec->repair_pkt_len;
}

static int a2dp_stream_error_filter(struct a2dp_dec_hdl *dec, u8 new_packet, u8 *packet, int len)
{
    int err = 0;

    if (dec->coding_type == AUDIO_CODING_AAC) {
        dec->header_len = get_rtp_header_len(dec->new_frame, packet, len);
        dec->new_frame = 0;
    } else {
        dec->header_len = get_rtp_header_len(1, packet, len);
    }

    if (dec->header_len >= len) {
        printf("##A2DP header error : %d\n", dec->header_len);
        a2dp_decoder_stream_free(dec, packet);
        return -EFAULT;
    }

    u16 seqn = RB16(packet + 2);
    if (new_packet) {
        if (dec->stream_error == A2DP_STREAM_UNDERRUN) {
            int missed_frames = (u16)(seqn - dec->seqn) - 1;
            if (missed_frames > dec->repair_frames) {
                dec->stream_error = A2DP_STREAM_MISSED;
                dec->missed_num = missed_frames - dec->repair_frames + 1;
                /*printf("case 0 : %d, %d\n", missed_frames, dec->repair_frames);*/
                err = -EAGAIN;
            } else if (missed_frames < dec->repair_frames) {
                dec->stream_error = A2DP_STREAM_OVERRUN;
                dec->overrun_seqn = seqn + dec->repair_frames - missed_frames;
                /*printf("case 1 : %d, %d, seqn : %d, %d\n", missed_frames, dec->repair_frames, seqn, dec->overrun_seqn);*/
                err = -EAGAIN;
            }
        } else if (!dec->stream_error && (u16)(seqn - dec->seqn) > 1) {
            dec->stream_error = A2DP_STREAM_MISSED;
            if (a2dp_audio_delay_time(dec) < dec->delay_time) {
                dec->missed_num = (u16)(seqn - dec->seqn);
                err = -EAGAIN;
            }
            int pkt_len;
            void *head = a2dp_media_fetch_packet(&pkt_len, NULL);
            /*printf("case 2 : %d, %d, pkt : 0x%x, 0x%x\n", seqn, dec->seqn, (u32)head, (u32)packet);*/
            if (dec->missed_num > 30) {
                printf("##A serious mistake : A2DP stream missed too much, %d\n", dec->missed_num);
                dec->missed_num = 30;
            }
        }
        dec->repair_frames = 0;
    }
    if (!err && new_packet) {
        dec->seqn = seqn;
    }
    dec->repair_pkt = packet;
    dec->repair_pkt_len = len;
    return err;
}

AT(.bt_decode.text.cache.L2.a2dp)
static int a2dp_dec_get_frame(struct audio_decoder *decoder, u8 **frame)
{
    struct a2dp_dec_hdl *dec = container_of(decoder, struct a2dp_dec_hdl, decoder);
    u8 *packet = NULL;
    int len = 0;
    u8 new_packet = 0;

try_again:
    switch (dec->stream_error) {
    case A2DP_STREAM_OVERRUN:
        new_packet = a2dp_stream_overrun_handler(dec, &packet, &len);
        break;
    case A2DP_STREAM_MISSED:
        new_packet = a2dp_stream_missed_handler(dec, &packet, &len);
        break;
    default:
        len = a2dp_media_try_get_packet(&packet);
        if (len <= 0) {
            len = a2dp_stream_underrun_handler(dec, &packet);
        } else {
            a2dp_decoder_stream_free(dec, NULL);
            new_packet = 1;
        }
        break;
    }

    if (len <= 0) {
        return 0;
    }

    int err = a2dp_stream_error_filter(dec, new_packet, packet, len);
    if (err) {
        if (-err == EAGAIN) {
            dec->new_frame = 1;
            goto try_again;
        }
        return 0;
    }

    *frame = packet + dec->header_len;
    len -= dec->header_len;
    if (dec->stream_error && new_packet) {
#if AUDIO_CODEC_SUPPORT_SYNC && TCFG_USER_TWS_ENABLE
        if (dec->ts_handle) {
            tws_a2dp_share_timestamp(dec->ts_handle);
        }
#endif
        dec->stream_error = 0;
    }

    if (dec->slience_frames) {
        dec->slience_frames--;
    }
    a2dp_decoder_set_timestamp(dec, dec->seqn);

    return len;
}

AT(.bt_decode.text.cache.L2.a2dp)
static void a2dp_dec_put_frame(struct audio_decoder *decoder, u8 *frame)
{
    struct a2dp_dec_hdl *dec = container_of(decoder, struct a2dp_dec_hdl, decoder);

    if (frame) {
        if (!a2dp_media_channel_exist() || app_var.goto_poweroff_flag) {
            a2dp_decoder_stream_free(dec, (void *)(frame - dec->header_len));
        }
        /*a2dp_media_free_packet((void *)(frame - dec->header_len));*/
    }
}

static int a2dp_dec_fetch_frame(struct audio_decoder *decoder, u8 **frame)
{
    struct a2dp_dec_hdl *dec = container_of(decoder, struct a2dp_dec_hdl, decoder);
    u8 *packet = NULL;
    int len = 0;
    u32 wait_timeout = 0;

    if (!dec->start) {
        wait_timeout = jiffies + msecs_to_jiffies(500);
    }

    dec->fetch_lock = 1;
__retry_fetch:
    packet = a2dp_media_fetch_packet(&len, NULL);
    if (packet) {
        dec->header_len = get_rtp_header_len(1, packet, len);
        *frame = packet + dec->header_len;
        len -= dec->header_len;
    } else if (!dec->start) {
        if (time_before(jiffies, wait_timeout)) {
            os_time_dly(1);
            goto __retry_fetch;
        }
    }

    dec->fetch_lock = 0;
    return len;
}

static const struct audio_dec_input a2dp_input = {
    .coding_type = AUDIO_CODING_SBC,
    .data_type   = AUDIO_INPUT_FRAME,
    .ops = {
        .frame = {
            .fget = a2dp_dec_get_frame,
            .fput = a2dp_dec_put_frame,
            .ffetch = a2dp_dec_fetch_frame,
        }
    }
};

#define bt_time_before(t1, t2) \
         (((t1 < t2) && ((t2 - t1) & 0x7ffffff) < 0xffff) || \
          ((t1 > t2) && ((t1 - t2) & 0x7ffffff) > 0xffff))
#define bt_time_to_msecs(clk)   (((clk) * 625) / 1000)
#define msecs_to_bt_time(m)     (((m + 1)* 1000) / 625)

AT(.bt_decode.text.cache.L2.a2dp)
static int a2dp_audio_delay_time(struct a2dp_dec_hdl *dec)
{
    /*struct a2dp_dec_hdl *dec = container_of(decoder, struct a2dp_dec_hdl, decoder);*/
    int msecs = 0;

#if TCFG_USER_TWS_ENABLE
    msecs = a2dp_media_get_remain_play_time(1);
#else
    msecs = a2dp_media_get_remain_play_time(0);
#endif

    if (dec->syncts) {
        msecs += sound_buffered_between_syncts_and_device(dec->syncts, 0);
    }

#if defined(TCFG_AUDIO_OUTPUT_IIS) && TCFG_AUDIO_OUTPUT_IIS
    msecs += audio_iis_data_time();
#else
    msecs += audio_dac_data_time(&dac_hdl);
#endif

    return msecs;
}

AT(.bt_decode.text.cache.L2.a2dp)
static int a2dp_decoder_stream_delay_update(struct audio_decoder *decoder)
{
    struct a2dp_dec_hdl *dec = container_of(decoder, struct a2dp_dec_hdl, decoder);
    int msecs = 0;
    int err = 0;

#if AUDIO_CODEC_SUPPORT_SYNC
    msecs = a2dp_audio_delay_time(dec);
    if (dec->stream_error) {
        return 0;
    }

    if (dec->sync_step == 2) {
        int distance_time = msecs - a2dp_delay_time;
        if (a2dp_bt_rx_overrun() && distance_time < 50) {
            distance_time = 50;
        }

        if (dec->ts_handle) {
            a2dp_audio_delay_offset_update(dec->ts_handle, distance_time);
        }
    }

#endif
    /*printf("%d -> %dms, delay_time : %dms\n", msecs1, msecs, a2dp_delay_time);*/
    return 0;
}

/*
 * A2DP 音频同步控制处理函数
 * 1.包括音频延时浮动参数;
 * 2.处理因为超时等情况丢弃音频样点;
 * 3.调用与蓝牙主机音频延时做同步的功能;
 * 4.处理TWS从机加入与解码被打断的情况。
 *
 */
AT(.bt_decode.text.cache.L2.a2dp)
static int a2dp_decoder_audio_sync_handler(struct audio_decoder *decoder)
{
    struct a2dp_dec_hdl *dec = container_of(decoder, struct a2dp_dec_hdl, decoder);
    int err;

    if (!dec->syncts) {
        return 0;
    }


    err = a2dp_decoder_stream_delay_update(decoder);
    if (err) {
        audio_decoder_suspend(decoder, 0);
        return -EAGAIN;
    }

    return 0;
}

static u16 a2dp_max_interval = 0;
#define A2DP_EST_AUDIO_CAPACITY             350//ms

static void a2dp_stream_underrun_feedback(void *priv)
{
    struct a2dp_dec_hdl *dec = (struct a2dp_dec_hdl *)priv;

    dec->underrun_feedback = 1;

    if (a2dp_delay_time < a2dp_max_interval + 50) {
        a2dp_delay_time = a2dp_max_interval + 50;
    } else {
        a2dp_delay_time += 50;
    }
    if (a2dp_delay_time > A2DP_EST_AUDIO_CAPACITY) {
        a2dp_delay_time = A2DP_EST_AUDIO_CAPACITY;
    }
}

/*void a2dp_stream_interval_time_handler(int time)*/
void reset_a2dp_sbc_instant_time(u16 time)
{
    if (a2dp_max_interval < time) {
        a2dp_max_interval = time;
#if A2DP_FLUENT_STREAM_MODE
        if (a2dp_max_interval > a2dp_delay_time) {
            a2dp_delay_time = a2dp_max_interval + 5;
            if (a2dp_delay_time > A2DP_EST_AUDIO_CAPACITY) {
                a2dp_delay_time = A2DP_EST_AUDIO_CAPACITY;
            }
        }
#endif
        /* y_printf("Max : %dms\n", time); */
    }
}

static void a2dp_stream_stability_detect(void *priv)
{
    struct a2dp_dec_hdl *dec = (struct a2dp_dec_hdl *)priv;

    if (dec->underrun_feedback) {
        dec->underrun_feedback = 0;
        return;
    }

    if (a2dp_delay_time > dec->delay_time) {
        if (a2dp_max_interval < a2dp_delay_time) {
            a2dp_delay_time -= 50;
            if (a2dp_delay_time < dec->delay_time) {
                a2dp_delay_time = dec->delay_time;
            }

            if (a2dp_delay_time < a2dp_max_interval) {
                a2dp_delay_time = a2dp_max_interval + 5;
            }
        }
        a2dp_max_interval = dec->delay_time;
    }
}

#if AUDIO_CODEC_SUPPORT_SYNC
static void a2dp_decoder_update_base_time(struct a2dp_dec_hdl *dec)
{
    int distance_time = a2dp_low_latency ? a2dp_delay_time : (a2dp_delay_time - a2dp_media_get_remain_play_time(1));
    if (!a2dp_low_latency) {
        distance_time = a2dp_delay_time;
    } else if (distance_time < 20) {
        distance_time = 20;
    }
    dec->base_time = bt_audio_sync_lat_time() + msecs_to_bt_time(distance_time);
}

#endif
static int a2dp_decoder_stream_is_available(struct a2dp_dec_hdl *dec)
{
    int err = 0;
    u8 *packet = NULL;
    int len = 0;
    int drop = 0;

#if AUDIO_CODEC_SUPPORT_SYNC
    if (dec->sync_step) {
        return 0;
    }

    packet = (u8 *)a2dp_media_fetch_packet(&len, NULL);
    if (!packet) {
        return -EINVAL;
    }

    dec->seqn = RB16(packet + 2);
    if (dec->ts_handle) {
#if TCFG_USER_TWS_ENABLE
        if (!tws_network_audio_was_started() && !a2dp_audio_timestamp_is_available(dec->ts_handle, dec->seqn, 0, &drop)) {
            if (drop) {
                local_irq_disable();
                u8 *check_packet = (u8 *)a2dp_media_fetch_packet(&len, NULL);
                if (check_packet && RB16(check_packet + 2) == dec->seqn) {
                    a2dp_media_free_packet(packet);
                }
                local_irq_enable();
                a2dp_decoder_update_base_time(dec);
                a2dp_audio_set_base_time(dec->ts_handle, dec->base_time);
            }
            return -EINVAL;
        }
#endif
    }
    dec->sync_step = 2;
#endif
    return 0;
}

AT(.bt_decode.text.cache.L2.a2dp)
static int a2dp_dec_probe_handler(struct audio_decoder *decoder)
{
    struct a2dp_dec_hdl *dec = container_of(decoder, struct a2dp_dec_hdl, decoder);
    int err = 0;

    err = a2dp_decoder_stream_is_available(dec);
    if (err) {
        audio_decoder_suspend(decoder, 0);
        return err;
    }

    err = a2dp_decoder_audio_sync_handler(decoder);
    if (err) {
        audio_decoder_suspend(decoder, 0);
        return err;
    }

    dec->new_frame = 1;

    a2dp_dec_set_output_channel(dec);

    return err;
}

#if (SYS_VOL_TYPE == VOL_TYPE_DIGITAL)
void a2dp_digital_vol_set(u8 vol)
{
    if (a2dp_dec) {
        audio_digital_vol_set(a2dp_dec->dvol, vol);
    }
}
#endif

AT(.bt_decode.text.cache.L2.a2dp)
static int a2dp_decoder_eq_drc_filter(void *priv, void *data, int len)
{
#if TCFG_BT_MUSIC_EQ_ENABLE
#if A2DP_EQ_SUPPORT_ASYNC
    if (a2dp_eq) {
        int eqlen = 0;
#if AUDIO_CODEC_SUPPORT_SYNC
        if (0) {//len > sizeof(dac_sync_buff)) {
            /*大于异步SYNC的buf按SYNC的buffer大小做异步eq，防止异步EQ存入过多延时*/
            eqlen = audio_eq_run(a2dp_eq, data, sizeof(dac_sync_buff));
        } else {
            eqlen = audio_eq_run(a2dp_eq, data, len);
        }
#else
        eqlen = audio_eq_run(a2dp_eq, data, len);
#endif
        return eqlen;
    }
#endif
#endif
    return len;
}

AT(.bt_decode.text.cache.L2.a2dp)
static int a2dp_output_after_syncts_filter(void *priv, void *data, int len)
{
    int wlen = 0;
    int drop_len = 0;
    struct a2dp_dec_hdl *dec = (struct a2dp_dec_hdl *)priv;

#if TCFG_BT_MUSIC_EQ_ENABLE
    return a2dp_decoder_eq_drc_filter(dec, data, len);
#endif
    return audio_mixer_ch_write(&dec->mix_ch, data, len);
}

AT(.bt_decode.text.cache.L2.a2dp)
static void a2dp_output_before_syncts_handler(struct a2dp_dec_hdl *dec, void *data, int len)
{
#if AUDIO_CODEC_SUPPORT_SYNC
    if (dec->ts_start == 1) {
        u32 timestamp = dec->timestamp;
        u32 current_time = ((bt_audio_sync_lat_time() + (30000 / 625)) & 0x7ffffff) * 625 * TIME_US_FACTOR;
#define time_after_timestamp(t1, t2) \
        ((t1 > t2 && ((t1 - t2) < (10000000L * TIME_US_FACTOR))) || (t1 < t2 && ((t2 - t1) > (10000000L * TIME_US_FACTOR))))
        if (!time_after_timestamp(timestamp, current_time)) {
            audio_syncts_resample_suspend(dec->syncts);
        } else {
            audio_syncts_resample_resume(dec->syncts);
            dec->mix_ch_event_params[2] = timestamp;
            dec->ts_start = 2;
        }
    }
#endif
}

AT(.bt_decode.text.cache.L2.a2dp)
static void audio_filter_resume_decoder(void *priv)
{
    struct audio_decoder *decoder = (struct audio_decoder *)priv;

    audio_decoder_resume(decoder);
}

AT(.bt_decode.text.cache.L2.a2dp)
static int a2dp_decoder_slience_plc_filter(struct a2dp_dec_hdl *dec, void *data, int len)
{
    if (len == 0) {
        a2dp_decoder_stream_free(dec, NULL);
        if (!dec->stream_error) {
            dec->stream_error = A2DP_STREAM_DECODE_ERR;
            dec->repair = 1;
        }
        return 0;
    }
    if (dec->stream_error) {
        memset(data, 0x0, len);
    }
#if TCFG_USER_TWS_ENABLE
    u8 tws_pairing = 0;
    if (dec->preempt || tws_network_audio_was_started()) {
        /*a2dp播放中副机加入，声音淡入进去*/
        tws_network_local_audio_start();
        dec->preempt = 0;
        tws_pairing = 1;
        memset(data, 0x0, len);
        dec->slience_frames = 30;
    }
#endif
#if A2DP_AUDIO_PLC_ENABLE
    if (dec->plc_ops) {
        if (dec->slience_frames) {
            dec->plc_ops->run(dec->plc_mem, data, data, len >> 1, 2);
        } else if (dec->stream_error) {
            dec->plc_ops->run(dec->plc_mem, data, data, len >> 1, dec->repair ? 1 : 2);
            dec->repair = 0;
        } else {
            dec->plc_ops->run(dec->plc_mem, data, data, len >> 1, 0);
        }
    }
#endif
    return len;
}

AT(.bt_decode.text.cache.L2.a2dp)
static int a2dp_dec_output_handler(struct audio_decoder *decoder, s16 *data, int len, void *priv)
{
    int wlen = 0;
    struct a2dp_dec_hdl *dec = container_of(decoder, struct a2dp_dec_hdl, decoder);

    if (!dec->remain) {
        wlen = a2dp_decoder_slience_plc_filter(dec, data, len);
        if (wlen < len) {
            return wlen;
        }
#if (SYS_VOL_TYPE == VOL_TYPE_DIGITAL)
        audio_digital_vol_run(dec->dvol, data, len);
#endif

#if TCFG_DIG_PHASE_INVERTER_EN
        digital_phase_inverter_s16(data, len);
#endif/*TCFG_DIG_PHASE_INVERTER_EN*/

#if  (TCFG_AUDIO_BASS_BOOST && TCFG_DRC_ENABLE)
        if (a2dp_wdrc && wdrc_run) {
            audio_drc_run(a2dp_wdrc, data, len);
        }
#endif/*TCFG_AUDIO_BASS_BOOST*/
#if AUDIO_SURROUND_CONFIG
        if (dec->surround) {
            audio_surround_run(dec->surround, data, len);
        }
#endif

#if AUDIO_VBASS_CONFIG
        if (dec->vbass) {
            audio_vbass_run(dec->vbass, data, len);
        }
#endif
        a2dp_output_before_syncts_handler(dec, data, len);
    }
#if AUDIO_CODEC_SUPPORT_SYNC
    if (dec->syncts) {
        int wlen = audio_syncts_frame_filter(dec->syncts, data, len);
        if (wlen < len) {
            audio_syncts_trigger_resume(dec->syncts, (void *)decoder, audio_filter_resume_decoder);
        }
        dec->remain = wlen < len ? 1 : 0;
        return wlen;
    }
#endif
    wlen = a2dp_output_after_syncts_filter(dec, data, len);
    dec->remain = wlen < len ? 1 : 0;
    return wlen;
}


static int a2dp_dec_post_handler(struct audio_decoder *decoder)
{
    return 0;
}

static const struct audio_dec_handler a2dp_dec_handler = {
    .dec_probe  = a2dp_dec_probe_handler,
    .dec_output = a2dp_dec_output_handler,
    .dec_post   = a2dp_dec_post_handler,
};

void a2dp_dec_close();

static void a2dp_dec_release()
{
    audio_decoder_task_del_wait(&decode_task, &a2dp_dec->wait);
    a2dp_drop_frame_stop();

    local_irq_disable();
    free(a2dp_dec);
    a2dp_dec = NULL;
    local_irq_enable();
}

static void a2dp_dec_event_handler(struct audio_decoder *decoder, int argc, int *argv)
{
    switch (argv[0]) {
    case AUDIO_DEC_EVENT_END:
        puts("AUDIO_DEC_EVENT_END\n");
        a2dp_dec_close();
        break;
    }
}

#if ESCO_DRC_EN
struct drc_ch esco_drc_p = {0};
int esco_drc_get_filter_info(struct audio_drc_filter_info *info)
{

    int th = 0;//db -60db~0db,限幅器阈值
    int threshold = round(pow(10.0, th / 20.0) * 32768);
    esco_drc_p.nband = 1;
    esco_drc_p.type = 1;
    esco_drc_p._p.limiter[0].attacktime = 5;
    esco_drc_p._p.limiter[0].releasetime = 500;
    esco_drc_p._p.limiter[0].threshold[0] = threshold;
    esco_drc_p._p.limiter[0].threshold[1] = 32768;
    info->R_pch = info->pch = &esco_drc_p;
    return 0;
}
#endif
static void a2dp_decoder_delay_time_setup(struct a2dp_dec_hdl *dec)
{
#if TCFG_USER_TWS_ENABLE
    int a2dp_low_latency = tws_api_get_low_latency_state();
#endif
    if (a2dp_low_latency) {
        a2dp_delay_time = a2dp_dec->coding_type == AUDIO_CODING_AAC ? CONFIG_A2DP_DELAY_TIME_LO : CONFIG_A2DP_SBC_DELAY_TIME_LO;
    } else {
        a2dp_delay_time = CONFIG_A2DP_DELAY_TIME;
    }
    a2dp_max_interval = 0;

    dec->delay_time = a2dp_delay_time;

    dec->detect_timer = sys_timer_add((void *)dec, a2dp_stream_stability_detect, A2DP_FLUENT_DETECT_INTERVAL);
}

static void a2dp_decoder_eq_filter_setup(struct a2dp_dec_hdl *dec, struct audio_fmt *fmt)
{
#if TCFG_EQ_ENABLE
#if TCFG_BT_MUSIC_EQ_ENABLE
    a2dp_eq = zalloc(sizeof(struct audio_eq) + sizeof(struct hw_eq_ch));
    if (a2dp_eq) {
        a2dp_eq->eq_ch = (struct hw_eq_ch *)((int)a2dp_eq + sizeof(struct audio_eq));
        struct audio_eq_param a2dp_eq_param = {0};
        a2dp_eq_param.channels = dec->ch;//fmt->channel;
        a2dp_eq_param.online_en = 1;
        a2dp_eq_param.mode_en = 1;
        a2dp_eq_param.remain_en = 1;
#if A2DP_EQ_SUPPORT_ASYNC
#if TCFG_DRC_ENABLE && TCFG_BT_MUSIC_DRC_ENABLE
        a2dp_eq_param.no_wait =  1;
#else
        a2dp_eq_param.no_wait = a2dp_low_latency ? 0 : 1;
#endif
#endif
        a2dp_eq_param.max_nsection = EQ_SECTION_MAX;
        a2dp_eq_param.cb = eq_get_filter_info;
        audio_eq_open(a2dp_eq, &a2dp_eq_param);

#if defined(TCFG_AUDIO_OUTPUT_IIS) && TCFG_AUDIO_OUTPUT_IIS
        audio_eq_set_samplerate(a2dp_eq, TCFG_IIS_SAMPLE_RATE);
#else
        audio_eq_set_samplerate(a2dp_eq, fmt->sample_rate);
#endif
#if A2DP_EQ_SUPPORT_32BIT
        audio_eq_set_info(a2dp_eq, a2dp_eq_param.channels, 1);
#endif
        audio_eq_set_output_handle(a2dp_eq, a2dp_eq_output, dec);
        audio_eq_start(a2dp_eq);
    }
#endif
#endif
}

static void a2dp_decoder_drc_filter_setup(struct a2dp_dec_hdl *dec, struct audio_fmt *fmt)
{
#if TCFG_EQ_ENABLE
#if TCFG_BT_MUSIC_DRC_ENABLE
    a2dp_drc = malloc(sizeof(struct audio_drc));
    if (a2dp_drc) {
        struct audio_drc_param drc_param = {0};
        drc_param.channels = dec->ch;
        drc_param.online_en = 1;
        drc_param.remain_en = 1;
        drc_param.cb = drc_get_filter_info;
        audio_drc_open(a2dp_drc, &drc_param);
#if defined(TCFG_AUDIO_OUTPUT_IIS) && TCFG_AUDIO_OUTPUT_IIS
        audio_drc_set_samplerate(a2dp_drc, TCFG_IIS_SAMPLE_RATE);
#else
        audio_drc_set_samplerate(a2dp_drc, fmt->sample_rate);
#endif
#if A2DP_EQ_SUPPORT_32BIT
        audio_drc_set_32bit_mode(a2dp_drc, 1);
#endif
        audio_drc_set_output_handle(a2dp_drc, NULL, NULL);
        audio_drc_start(a2dp_drc);
    }
#endif
#endif
}

static void a2dp_decoder_surround_vbass_setup(struct a2dp_dec_hdl *dec, struct audio_fmt *fmt)
{
#if AUDIO_SURROUND_CONFIG
    u8 nch = EFFECT_CH_L;
    if (dec->channel == AUDIO_CH_L) {
        nch = EFFECT_CH_L;
    } else if (dec->channel == AUDIO_CH_R) {
        nch = EFFECT_CH_R;
    } else if (dec->channel == AUDIO_CH_LR) {
        nch = 2;
    }
    surround_open_parm parm = {0};
    parm.channel = nch;
    parm.surround_effect_type = EFFECT_3D_PANORAMA;//打开时默认使用3d全景音,使用者，根据需求修改
    dec->surround = audio_surround_open(&parm);
    dec->surround_eff = a2dp_surround_eff;
    audio_surround_voice(dec);//还原按键触发的音效
    //sur_test = sys_timer_add(dec->surround, surround_switch_test, 10000);
#endif


#if AUDIO_VBASS_CONFIG
    vbass_open_parm vparm = {0};
    vparm.sr = fmt->sample_rate;
    vparm.channel = dec->ch;
    dec->vbass = audio_vbass_open(&vparm);
    if (dec->vbass) {
        vbass_update_parm def_parm = {0};
        def_parm.bass_f = 200;//外放的低音截止频率
        def_parm.level = 4096;//增强强度(4096 等于 1db， 建议范围：4096 到 16384);
        audio_vbass_parm_update(dec->vbass, VBASS_UPDATE_PARM, &def_parm);

#if AUDIO_VBASS_TEST
        //vbass_t = sys_timer_add(dec->vbass, vbass_udate_parm_test, 2000);
#endif
    }
#endif
}

static int a2dp_decoder_syncts_setup(struct a2dp_dec_hdl *dec)
{
    int err = 0;
#if AUDIO_CODEC_SUPPORT_SYNC
    struct audio_syncts_params params = {0};

    params.nch = dec->ch;
#if defined(TCFG_AUDIO_OUTPUT_IIS) && TCFG_AUDIO_OUTPUT_IIS
    params.pcm_device = PCM_OUTSIDE_DAC;
    params.rout_sample_rate = TCFG_IIS_SAMPLE_RATE;
#else
    params.pcm_device = PCM_INSIDE_DAC;
    params.rout_sample_rate = dec->sample_rate;
#endif
    params.network = AUDIO_NETWORK_BT2_1;
    params.rin_sample_rate = dec->sample_rate;
    params.priv = dec;
    params.factor = TIME_US_FACTOR;
    params.output = a2dp_output_after_syncts_filter;

    bt_audio_sync_nettime_select(0);//0 - a2dp主机，1 - tws, 2 - BLE

    a2dp_decoder_update_base_time(dec);
    dec->ts_handle = a2dp_audio_timestamp_create(dec->sample_rate, dec->base_time, TIME_US_FACTOR);

    err = audio_syncts_open(&dec->syncts, &params);
    if (!err) {
        dec->mix_ch_event_params[0] = (u32)&dec->mix_ch;
        dec->mix_ch_event_params[1] = (u32)dec->syncts;
        dec->mix_ch_event_params[2] = dec->base_time * 625 * TIME_US_FACTOR;
        audio_mixer_ch_set_event_handler(&dec->mix_ch, (void *)dec->mix_ch_event_params, audio_mix_ch_event_handler);
    }

    dec->sync_step = 0;
#endif
    return err;
}

static void a2dp_decoder_syncts_free(struct a2dp_dec_hdl *dec)
{
#if AUDIO_CODEC_SUPPORT_SYNC
    if (dec->ts_handle) {
        a2dp_audio_timestamp_close(dec->ts_handle);
        dec->ts_handle = NULL;
    }
    if (dec->syncts) {
        audio_syncts_close(dec->syncts);
        dec->syncts = NULL;
    }
#endif
}

static int a2dp_decoder_plc_setup(struct a2dp_dec_hdl *dec)
{
#if A2DP_AUDIO_PLC_ENABLE
    int plc_mem_size;
    dec->plc_ops = get_lfaudioPLC_api();
    plc_mem_size = dec->plc_ops->need_buf(dec->ch); // 3660bytes，请衡量是否使用该空间换取PLC处理
    dec->plc_mem = malloc(plc_mem_size);
    if (!dec->plc_mem) {
        dec->plc_ops = NULL;
        return -ENOMEM;
    }
    dec->plc_ops->open(dec->plc_mem, dec->ch, a2dp_low_latency ? 0 : 4);
#endif
    return 0;
}

static void a2dp_decoder_plc_free(struct a2dp_dec_hdl *dec)
{
#if A2DP_AUDIO_PLC_ENABLE
    if (dec->plc_mem) {
        free(dec->plc_mem);
        dec->plc_mem = NULL;
    }
#endif
}

static void a2dp_decoder_match_system_clock(struct a2dp_dec_hdl *dec)
{
#if 0
    /*
     * A2DP解码时钟设置：
     * 1、普通设置
     *    开始解码: clk_set_sys_lock(xxx, 0);
     *    退出解码: clk_set_sys_lock(xxx, 0);
     * 2、特殊设置（使用wav或其他提示音并带有操作开始/暂停 A2DP音频）
     *    开始解码: clk_set_sys_lock(xxx, 1);
     *    退出解码: clk_set_sys_lock(xxx, 2);
     */
    if (dec->coding_type == AUDIO_CODING_SBC) {
#if (TCFG_BT_MUSIC_EQ_ENABLE && ((EQ_SECTION_MAX >= 10) && AUDIO_OUT_EFFECT_ENABLE))
        clk_set_sys_lock(SYS_64M, 1) ;
#elif (TCFG_BT_MUSIC_EQ_ENABLE && ((EQ_SECTION_MAX >= 10) || AUDIO_OUT_EFFECT_ENABLE))
        clk_set_sys_lock(SYS_48M, 1) ;
#else
#if (!TCFG_AUDIO_ANC_ENABLE)
        clk_set_sys_lock(SYS_24M, 1) ;
#endif
#endif
    } else if (dec->coding_type == AUDIO_CODING_AAC) {
#if (TCFG_BT_MUSIC_EQ_ENABLE && ((EQ_SECTION_MAX >= 10) || AUDIO_OUT_EFFECT_ENABLE))
        clk_set_sys_lock(SYS_64M, 1) ;
#else
        clk_set_sys_lock(SYS_48M, 1) ;
#endif
    }

#if (AUDIO_VBASS_CONFIG || AUDIO_SURROUND_CONFIG)
    clk_set_sys_lock(SYS_64M, 1) ;
#endif
#else
    audio_codec_clock_set(AUDIO_A2DP_MODE, dec->coding_type, dec->wait.preemption);
#endif
}

void a2dp_decoder_sample_detect_setup(struct a2dp_dec_hdl *dec)
{
    dec->sample_detect = a2dp_sample_detect_open(dec->sample_rate, dec->coding_type);
}

void a2dp_decoder_sample_detect_free(struct a2dp_dec_hdl *dec)
{
    if (dec->sample_detect) {
        a2dp_sample_detect_close(dec->sample_detect);
        dec->sample_detect = NULL;
    }
}

static u8 *code_run_addr = NULL;
extern u8 __a2dp_text_cache_L2_start[];
extern u8 __a2dp_text_cache_L2_end[];
extern u32 __a2dp_movable_slot_start[];
extern u32 __a2dp_movable_slot_end[];

void audio_overlay_load_code(u32 type);
int a2dp_dec_start()
{
    int err;
    struct audio_fmt *fmt;
    enum audio_channel channel;
    struct a2dp_dec_hdl *dec = a2dp_dec;

    if (!a2dp_dec) {
        return -EINVAL;
    }

    a2dp_drop_frame_stop();
    puts("a2dp_dec_start: in\n");

    if (a2dp_dec->coding_type == AUDIO_CODING_AAC) {
        audio_overlay_load_code(a2dp_dec->coding_type);
    }

    err = audio_decoder_open(&dec->decoder, &a2dp_input, &decode_task);
    if (err) {
        goto __err1;
    }
    dec->channel = AUDIO_CH_MAX;

    audio_decoder_set_handler(&dec->decoder, &a2dp_dec_handler);
    audio_decoder_set_event_handler(&dec->decoder, a2dp_dec_event_handler, 0);

    if (a2dp_dec->coding_type != a2dp_input.coding_type) {
        struct audio_fmt f = {0};
        f.coding_type = a2dp_dec->coding_type;
        err = audio_decoder_set_fmt(&dec->decoder, &f);
        if (err) {
            goto __err2;
        }
    }

    err = audio_decoder_get_fmt(&dec->decoder, &fmt);
    if (err) {
        goto __err2;
    }

    if (fmt->sample_rate == 0) {
        log_w("A2DP stream maybe error\n");
        goto __err2;
    }
    //dac_hdl.dec_channel_num = fmt->channel;
    dec->sample_rate = fmt->sample_rate;

    a2dp_decoder_delay_time_setup(dec);
    set_source_sample_rate(fmt->sample_rate);
    a2dp_dec_set_output_channel(dec);

    /*audio_dac_set_protect_time(&dac_hdl, 3, dec, NULL);//a2dp_stream_underrun_feedback);*/

    audio_mixer_ch_open(&dec->mix_ch, &mixer);
#if defined(TCFG_AUDIO_OUTPUT_IIS) && TCFG_AUDIO_OUTPUT_IIS
    audio_mixer_ch_set_sample_rate(&dec->mix_ch, TCFG_IIS_SAMPLE_RATE);
#else
    audio_mixer_ch_set_sample_rate(&dec->mix_ch, fmt->sample_rate);
#endif
    audio_mixer_ch_set_resume_handler(&dec->mix_ch, (void *)&dec->decoder, (void (*)(void *))audio_decoder_resume);

    app_audio_state_switch(APP_AUDIO_STATE_MUSIC, get_max_sys_vol());
#if (SYS_VOL_TYPE == VOL_TYPE_DIGITAL)
    a2dp_dec->dvol = audio_digital_vol_open(app_audio_get_volume(APP_AUDIO_STATE_MUSIC), SYS_MAX_VOL, 2);
#endif

    a2dp_decoder_sample_detect_setup(dec);

    a2dp_decoder_wdrc_filter_setup(dec, fmt);
    a2dp_decoder_eq_filter_setup(dec, fmt);
    a2dp_decoder_drc_filter_setup(dec, fmt);
    a2dp_decoder_surround_vbass_setup(dec, fmt);
    a2dp_decoder_syncts_setup(dec);
    a2dp_decoder_plc_setup(dec);

    dec->remain = 0;
    dec->stream_error = 0;
    a2dp_decoder_match_system_clock(dec);
    err = audio_decoder_start(&dec->decoder);
    if (err) {
        goto __err3;
    }

    dec->start = 1;
    mem_stats();

    int code_size = __a2dp_text_cache_L2_end - __a2dp_text_cache_L2_start;
    if (code_size && code_run_addr == NULL) {
        code_run_addr = phy_malloc(code_size);
        printf("---------load_code[a2dp]: %x, %x, %x, %x, %x\n", code_run_addr,
               __a2dp_text_cache_L2_start, code_size,
               __a2dp_movable_slot_start, __a2dp_movable_slot_end);
        ASSERT(code_run_addr != NULL);
        if (code_run_addr) {
            code_movable_load(__a2dp_text_cache_L2_start, code_size, code_run_addr,
                              __a2dp_movable_slot_start, __a2dp_movable_slot_end);
        }
    }

    return 0;

__err3:
    audio_mixer_ch_close(&a2dp_dec->mix_ch);
__err2:
    audio_decoder_close(&dec->decoder);
__err1:
    a2dp_dec_release();

    return err;
}

static int __a2dp_audio_res_close(void)
{
    a2dp_dec->start = 0;
    if (a2dp_dec->detect_timer) {
        sys_timer_del(a2dp_dec->detect_timer);
    }
    a2dp_decoder_sample_detect_free(a2dp_dec);
    audio_decoder_close(&a2dp_dec->decoder);
    audio_mixer_ch_close(&a2dp_dec->mix_ch);
    a2dp_decoder_syncts_free(a2dp_dec);
    a2dp_decoder_plc_free(a2dp_dec);
    a2dp_decoder_stream_free(a2dp_dec, NULL);


#if (TCFG_AUDIO_BASS_BOOST && TCFG_DRC_ENABLE)
    if (a2dp_wdrc) {
        audio_drc_close(a2dp_wdrc);
        free(a2dp_wdrc);
        a2dp_wdrc = NULL;
    }
#endif/*TCFG_AUDIO_BASS_BOOST*/

#if TCFG_BT_MUSIC_EQ_ENABLE
    if (a2dp_eq) {
        audio_eq_close(a2dp_eq);
        free(a2dp_eq);
        a2dp_eq = NULL;
    }
#endif

#if TCFG_BT_MUSIC_DRC_ENABLE
    if (a2dp_drc) {
        audio_drc_close(a2dp_drc);
        free(a2dp_drc);
        a2dp_drc = NULL;
    }
#endif

#if A2DP_EQ_SUPPORT_32BIT
    if (a2dp_dec->eq_out_buf) {
        free(a2dp_dec->eq_out_buf);
        a2dp_dec->eq_out_buf = NULL;
    }
#endif

#if AUDIO_SURROUND_CONFIG
    if (a2dp_dec->surround) {
        audio_surround_close(a2dp_dec->surround);
        a2dp_dec->surround = NULL;
    }
#endif

#if AUDIO_VBASS_CONFIG
    if (a2dp_dec->vbass) {
        audio_vbass_close(a2dp_dec->vbass);
        a2dp_dec->vbass = NULL;
    }
#endif


#if (SYS_VOL_TYPE == VOL_TYPE_DIGITAL)
    audio_digital_vol_close(a2dp_dec->dvol);
    a2dp_dec->dvol = NULL;
#endif
#if 0
    clk_set_sys_lock(BT_NORMAL_HZ, 0);
#else
    audio_codec_clock_del(AUDIO_A2DP_MODE);
#endif

    a2dp_dec->preempt = 1;
    app_audio_state_exit(APP_AUDIO_STATE_MUSIC);
    return 0;
}

static int a2dp_wait_res_handler(struct audio_res_wait *wait, int event)
{
    int err = 0;

    printf("a2dp_wait_res_handler: %d\n", event);

    if (event == AUDIO_RES_GET) {
        err = a2dp_dec_start();
    } else if (event == AUDIO_RES_PUT) {
        if (a2dp_dec->start) {
            __a2dp_audio_res_close();
            a2dp_drop_frame_start();
        }
    }
    return err;
}

#define A2DP_CODEC_SBC			0x00
#define A2DP_CODEC_MPEG12		0x01
#define A2DP_CODEC_MPEG24		0x02
int __a2dp_dec_open(int media_type, u8 resume)
{
    struct a2dp_dec_hdl *dec;

    if (strcmp(os_current_task(), "app_core") != 0) {
        log_e("a2dp dec open in task : %s\n", os_current_task());
    }

#if TCFG_AUDIO_ANC_ENABLE
#ifdef CONFIG_ANC_OVERLAY
    if (anc_train_open_query()) {
        printf("anc_train_open,suspend a2dp_dec\n");
        if (drop_a2dp_timer == 0) {
            drop_a2dp_timer = sys_timer_add(NULL, __a2dp_drop_frame, 50);
        }
        return 0;
    }
#endif/*CONFIG_ANC_OVERLAY*/
#endif /*TCFG_AUDIO_ANC_ENABLE*/

    if (a2dp_suspend) {
        if (tws_api_get_role() == TWS_ROLE_MASTER) {
            if (drop_a2dp_timer == 0) {
                drop_a2dp_timer = sys_timer_add(NULL,
                                                a2dp_media_clear_packet_before_seqn, 100);
            }
        }
        return 0;
    }

    if (a2dp_dec) {
        return 0;
    }

    media_type = a2dp_media_get_codec_type();
    printf("a2dp_dec_open: %d\n", media_type);

    dec = zalloc(sizeof(*dec));
    if (!dec) {
        return -ENOMEM;
    }

    switch (media_type) {
    case A2DP_CODEC_SBC:
        printf("a2dp_media_type:SBC");
        dec->coding_type = AUDIO_CODING_SBC;
        break;
    case A2DP_CODEC_MPEG24:
        printf("a2dp_media_type:AAC");
        dec->coding_type = AUDIO_CODING_AAC;
        break;
    default:
        printf("a2dp_media_type unsupoport:%d", media_type);
        free(dec);
        return -EINVAL;
    }

#if TCFG_USER_TWS_ENABLE
    a2dp_low_latency = tws_api_get_low_latency_state();
#endif
    a2dp_dec = dec;
    dec->preempt = resume ? 1 : 0;
    dec->wait.priority = 0;
    dec->wait.preemption = 1;
    dec->wait.handler = a2dp_wait_res_handler;
    audio_decoder_task_add_wait(&decode_task, &dec->wait);

    if (a2dp_dec && (a2dp_dec->start == 0)) {
        a2dp_drop_frame_start();
    }

    return 0;
}

int a2dp_dec_open(int media_type)
{
    return __a2dp_dec_open(media_type, 0);
}

void a2dp_dec_close()
{
    if (!a2dp_dec) {
        if (CONFIG_LOW_LATENCY_ENABLE) {
            a2dp_low_latency_seqn = 0;
        }
        if (drop_a2dp_timer) {
            sys_timer_del(drop_a2dp_timer);
            drop_a2dp_timer = 0;
        }
        return;
    }

    if (a2dp_dec->start) {
        __a2dp_audio_res_close();
    }

    a2dp_dec_release();

    if (code_run_addr) {
        code_movable_unload(__a2dp_text_cache_L2_start,
                            __a2dp_movable_slot_start, __a2dp_movable_slot_end);
        phy_free(code_run_addr);
        code_run_addr = NULL;
        printf("----unload_code[a2dp]\n");
    }
    puts("a2dp_dec_close: exit\n");
}


static void a2dp_low_latency_clear_a2dp_packet(u8 *data, int len, int rx)
{
    if (rx) {
        a2dp_low_latency_seqn = (data[0] << 8) | data[1];
    }
}
REGISTER_TWS_FUNC_STUB(audio_dec_clear_a2dp_packet) = {
    .func_id = 0x132A6578,
    .func    = a2dp_low_latency_clear_a2dp_packet,
};


static void low_latency_drop_a2dp_frame(void *p)
{
    int len;

    /*y_printf("low_latency_drop_a2dp_frame\n");*/

    if (a2dp_low_latency_seqn == 0) {
        a2dp_media_clear_packet_before_seqn(0);
        return;
    }
    while (1) {
        u8 *packet = a2dp_media_fetch_packet(&len, NULL);
        if (!packet) {
            return;
        }
        u16 seqn = (packet[2] << 8) | packet[3];
        if (seqn_after(seqn, a2dp_low_latency_seqn)) {
            printf("clear_end: %d\n", seqn);
            break;
        }
        a2dp_media_free_packet(packet);
        /*printf("clear: %d\n", seqn);*/
    }

    if (drop_a2dp_timer) {
        sys_timer_del(drop_a2dp_timer);
        drop_a2dp_timer = 0;
    }
    int type = a2dp_media_get_codec_type();
    if (type >= 0) {
        /*a2dp_dec_open(type);*/
        __a2dp_dec_open(type, 1);
    }

    if (a2dp_low_latency == 0) {
#if A2DP_PLAY_AUTO_ROLE_SWITCH_ENABLE
        tws_api_auto_role_switch_enable();
#endif
    }

    printf("a2dp_delay: %d\n", a2dp_media_get_remain_play_time(1));
}


int earphone_a2dp_codec_set_low_latency_mode(int enable, int msec)
{
    int ret = 0;
    int len, err;

    if (CONFIG_LOW_LATENCY_ENABLE == 0) {
        return -EINVAL;
    }

    if (esco_dec) {
        return -EINVAL;
    }
    if (drop_a2dp_timer) {
        return -EINVAL;
    }
    if (a2dp_suspend) {
        return -EINVAL;
    }

    a2dp_low_latency = enable;
    a2dp_low_latency_seqn = 0;

    r_printf("a2dp_low_latency: %d, %d, %d\n", a2dp_dec->seqn, a2dp_delay_time, enable);

    if (!a2dp_dec || a2dp_dec->start == 0) {
#if TCFG_USER_TWS_ENABLE
        tws_api_low_latency_enable(enable);
#endif
        return 0;
    }

    if (a2dp_dec->coding_type == AUDIO_CODING_SBC) {
        a2dp_low_latency_seqn = a2dp_dec->seqn + (msec + a2dp_delay_time) / 15;
    } else {
        a2dp_low_latency_seqn = a2dp_dec->seqn + (msec + a2dp_delay_time) / 20;
    }

#if TCFG_USER_TWS_ENABLE
    u8 data[2];
    data[0] = a2dp_low_latency_seqn >> 8;
    data[1] = a2dp_low_latency_seqn;
    err = tws_api_send_data_to_slave(data, 2, 0x132A6578);
    if (err == -ENOMEM) {
        return -EINVAL;
    }
#endif

    a2dp_dec_close();

    a2dp_media_clear_packet_before_seqn(0);

#if TCFG_USER_TWS_ENABLE
    if (enable) {
        tws_api_auto_role_switch_disable();
    }
    tws_api_low_latency_enable(enable);
#endif

    drop_a2dp_timer = sys_timer_add(NULL, low_latency_drop_a2dp_frame, 40);

    /*r_printf("clear_to_seqn: %d\n", a2dp_low_latency_seqn);*/

    return 0;
}

int earphone_a2dp_codec_get_low_latency_mode()
{
#if TCFG_USER_TWS_ENABLE
    return tws_api_get_low_latency_state();
#endif
    return a2dp_low_latency;
}


int a2dp_tws_dec_suspend(void *p)
{
    r_printf("a2dp_tws_dec_suspend\n");
    /*mem_stats();*/

    if (a2dp_suspend) {
        return -EINVAL;
    }
    a2dp_suspend = 1;

    if (a2dp_dec) {
        a2dp_dec_close();
        a2dp_media_clear_packet_before_seqn(0);
        if (tws_api_get_role() == 0) {
            drop_a2dp_timer = sys_timer_add(NULL, a2dp_media_clear_packet_before_seqn, 100);
        }
    }

    int err = audio_decoder_fmt_lock(&decode_task, AUDIO_CODING_AAC);
    if (err) {
        log_e("AAC_dec_lock_faild\n");
    }

    return err;
}


void a2dp_tws_dec_resume(void)
{
    r_printf("a2dp_tws_dec_resume\n");

    if (a2dp_suspend) {
        a2dp_suspend = 0;

        if (drop_a2dp_timer) {
            sys_timer_del(drop_a2dp_timer);
            drop_a2dp_timer = 0;
        }

        audio_decoder_fmt_unlock(&decode_task, AUDIO_CODING_AAC);

        int type = a2dp_media_get_codec_type();
        printf("codec_type: %d\n", type);
        if (type >= 0) {
            if (tws_api_get_role() == 0) {
                a2dp_media_clear_packet_before_seqn(0);
            }
            a2dp_resume_time = jiffies + msecs_to_jiffies(80);
            /*a2dp_dec_open(type);*/
            __a2dp_dec_open(type, 1);
        }
    }
}


static const u8 msbc_mute_data[] = {
    0xAD, 0x00, 0x00, 0xC5, 0x00, 0x00, 0x00, 0x00, 0x77, 0x6D, 0xB6, 0xDD, 0xDB, 0x6D, 0xB7, 0x76,
    0xDB, 0x6D, 0xDD, 0xB6, 0xDB, 0x77, 0x6D, 0xB6, 0xDD, 0xDB, 0x6D, 0xB7, 0x76, 0xDB, 0x6D, 0xDD,
    0xB6, 0xDB, 0x77, 0x6D, 0xB6, 0xDD, 0xDB, 0x6D, 0xB7, 0x76, 0xDB, 0x6D, 0xDD, 0xB6, 0xDB, 0x77,
    0x6D, 0xB6, 0xDD, 0xDB, 0x6D, 0xB7, 0x76, 0xDB, 0x6C, 0x00,
};

static int headcheck(u8 *buf)
{
    int sync_word = buf[0] | ((buf[1] & 0x0f) << 8);
    return (sync_word == 0x801) && (buf[2] == 0xAD);
}

static int esco_dump_rts_info(struct rt_stream_info *pkt)
{
    u32 hash = 0xffffffff;
    int read_len = 0;
    pkt->baddr = lmp_private_get_esco_packet(&read_len, &hash);
    pkt->seqn = hash;
    /* printf("hash0=%d,%d ",hash,pkt->baddr ); */
    if (pkt->baddr && read_len) {
        pkt->remain_len = lmp_private_get_esco_remain_buffer_size();
        pkt->data_len = lmp_private_get_esco_data_len();
        return 0;
    }
    if (read_len == -EINVAL) {
        //puts("----esco close\n");
        return -EINVAL;
    }
    if (read_len < 0) {

        return -ENOMEM;
    }
    return ENOMEM;
}

static int esco_dec_get_frame(struct audio_decoder *decoder, u8 **frame)
{
    int len = 0;
    u32 hash = 0;
    struct esco_dec_hdl *dec = container_of(decoder, struct esco_dec_hdl, decoder);

__again:
    if (dec->frame) {
        int len = dec->frame_len - dec->offset;
        if (len > dec->esco_len - dec->data_len) {
            len = dec->esco_len - dec->data_len;
        }
        /*memcpy((u8 *)dec->data + dec->data_len, msbc_mute_data, sizeof(msbc_mute_data));*/
        memcpy((u8 *)dec->data + dec->data_len, dec->frame + dec->offset, len);
        dec->offset   += len;
        dec->data_len += len;
        if (dec->offset == dec->frame_len) {
            lmp_private_free_esco_packet(dec->frame);
            dec->frame = NULL;
        }
    }

    if (dec->data_len < dec->esco_len) {
        dec->frame = lmp_private_get_esco_packet(&len, &hash);
        /* printf("hash1=%d,%d ",hash,dec->frame ); */
        if (len <= 0) {
            printf("rlen=%d ", len);
            return -EIO;
        }
#if AUDIO_CODEC_SUPPORT_SYNC
        u32 timestamp;
        if (dec->ts_handle) {
            timestamp = esco_audio_timestamp_update(dec->ts_handle, hash);
            if (dec->syncts && (((hash - dec->hash) & 0x7ffffff) == dec->frame_time)) {
                audio_syncts_next_pts(dec->syncts, timestamp);
            }
            dec->hash = hash;
            if (!dec->ts_start) {
                dec->ts_start = 1;
                dec->mix_ch_event_params[2] = timestamp;
            }
        }
#endif
#if (defined(TCFG_PHONE_MESSAGE_ENABLE) && (TCFG_PHONE_MESSAGE_ENABLE))
        phone_message_enc_write(dec->frame + 2, len - 2);
#endif
        dec->offset = 0;
        dec->frame_len = len;
        goto __again;
    }
    *frame = (u8 *)dec->data;
    return dec->esco_len;
}

static void esco_dec_put_frame(struct audio_decoder *decoder, u8 *frame)
{
    struct esco_dec_hdl *dec = container_of(decoder, struct esco_dec_hdl, decoder);

    dec->data_len = 0;
    /*lmp_private_free_esco_packet((void *)frame);*/
}

static const struct audio_dec_input esco_input = {
    .coding_type = AUDIO_CODING_MSBC,
    .data_type   = AUDIO_INPUT_FRAME,
    .ops = {
        .frame = {
            .fget = esco_dec_get_frame,
            .fput = esco_dec_put_frame,
        }
    }
};

u32 lmp_private_clear_sco_packet(u8 clear_num);
static void esco_dec_clear_all_packet(struct esco_dec_hdl *dec)
{
    lmp_private_clear_sco_packet(0xff);
}

static int esco_dec_rx_delay_monitor(struct audio_decoder *decoder, struct rt_stream_info *info)
{
    struct esco_dec_hdl *dec = container_of(decoder, struct esco_dec_hdl, decoder);

#if 0
    if (!dec->sync_step) {
        if (info->data_len <= 0) {
            return -EAGAIN;
        }
#if TCFG_USER_TWS_ENABLE
        int state = tws_api_get_tws_state();
        if (state & TWS_STA_SIBLING_CONNECTED) {
            if (tws_api_get_role() == TWS_ROLE_SLAVE) {
                audio_dac_sound_reset(&dac_hdl, 150);
            }
        }
#endif/*TCFG_USER_TWS_ENABLE*/
        dec->sync_step = 2;
    }

    if (info->data_len <= 120) {
        info->distance_time = -15;
    } else if (info->data_len > 240) {
        info->distance_time = 15;
    }

#endif
    /*printf("%d - %d\n", info->data_len, info->remain_len);*/
    return 0;
}

static int esco_dec_probe_handler(struct audio_decoder *decoder)
{
    struct esco_dec_hdl *dec = container_of(decoder, struct esco_dec_hdl, decoder);
    int err = 0;
    int find_packet = 0;
    struct rt_stream_info rts_info = {0};
    err = esco_dump_rts_info(&rts_info);
    if (err == -EINVAL) {
        return err;
    }
    if (err || !dec->enc_start) {
        audio_decoder_suspend(decoder, 0);
        return -EAGAIN;
    }

#if TCFG_USER_TWS_ENABLE
    if (tws_network_audio_was_started()) {
        /*清除从机声音加入标志*/
        dec->slience_frames = 20;
        tws_network_local_audio_start();
    }
#endif
    if (dec->preempt) {
        dec->preempt = 0;
        dec->slience_frames = 20;
    }
    return err;
}

#if TCFG_PHONE_EQ_ENABLE
static int esco_eq_output(void *priv, void *data, u32 len)
{
    return len;
}
#endif/*TCFG_PHONE_EQ_ENABLE*/

static int audio_esco_int2short_convert_output(void *priv, s16 *data, int len)
{
    /* put_buf(data,len); */
    return audio_dac_write(&dac_hdl, data, len);

}
static int audio_esco_short2int_convert_output(void *priv, void *data, int len)
{
#if 0//AUDIO_CODEC_SUPPORT_SYNC
    if (audio_sync) {
        audio_decoder_sync_run(audio_sync, data, len);
#if TCFG_PHONE_EQ_ENABLE
    } else if (esco_eq) {
        return audio_eq_run(esco_eq, data, len);
#endif
    } else if (int2short_convert) {
        /*return int2short_convert_run(int2short_convert, data, len);*/
    }
#endif
    return len;
}

/*level:0~15*/
static const u16 esco_dvol_tab[] = {
    0,	//0
    111,//1
    161,//2
    234,//3
    338,//4
    490,//5
    708,//6
    1024,//7
    1481,//8
    2142,//9
    3098,//10
    4479,//11
    6477,//12
    9366,//13
    14955,//14
    16384 //15
};
#if TCFG_ESCO_DL_NS_ENABLE
#include "commproc_ns.h"
#define NS_FRAME_POINTS	160
#define NS_FRAME_SIZE	(NS_FRAME_POINTS << 1)
typedef struct {
    /*s16 out[NS_FRAME_POINTS];*/
    s16 in[512];
    cbuffer_t cbuf;
    u16 offset;
} dl_ns_t;
/*dl_ns_t *dl_ns = NULL;*/

int esco_dl_ns_run(short *in, short *out, u16 len)
{
    /*struct dl_ns_t *dl_ns = (struct dl_ns_t *)priv;*/
    int out_len = noise_suppress_run(in, out, len / 2);
    return (out_len << 1);
}

static noise_suppress_param ns_para = {
    .wideband = 1,
    .mode = 0,
    .NoiseLevel = 1.0e2f,
    .AggressFactor = 1.25f,
    .MinSuppress = 0.04f,
};
int esco_dl_ns_open(struct esco_dec_hdl *dec, u16 sr)
{
    if (sr == 8000) {
        ns_para.wideband = 0;
    }
    noise_suppress_open(&ns_para);
    float lowcut = -60.f;
    noise_suppress_config(NS_CMD_LOWCUTTHR, 0, &lowcut);
    float noisefloor = -60.f;
    noise_suppress_config(NS_CMD_NOISE_FLOOR, 0, &noisefloor);
    dec->dl_ns_remain = 0;
    dec->dl_ns_offset = 0;
    printf("esco_dl_ns_open ok\n");
    return 0;
}

int esco_dl_ns_close(void)
{
    noise_suppress_close();
    return 0;
}
#endif/*TCFG_ESCO_DL_NS_ENABLE*/

#define MONO_TO_DUAL_POINTS 30


//////////////////////////////////////////////////////////////////////////////
static inline void audio_pcm_mono_to_dual(s16 *dual_pcm, s16 *mono_pcm, int points)
{
    s16 *mono = mono_pcm;
    int i = 0;
    u8 j = 0;

    for (i = 0; i < points; i++, mono++) {
        *dual_pcm++ = *mono;
        *dual_pcm++ = *mono;
    }
}

/*
*********************************************************************
*                  ESCO Decode mix filter
* Description: ESCO语音解码输出接入mix滤镜
* Arguments  : data - 音频数据，len - 音频byte长度
* Return	 : 输出到后级的数据长度
* Note(s)    : 音频后级PCM设备为单声道直接写入mixer，PCM设备为双声道，
*              这里通过1 to 2ch的处理将数据写入mixer，并返回消耗的原始
*              数据长度。
*********************************************************************
*/
static int esco_decoder_mix_filter(struct esco_dec_hdl *dec, s16 *data, int len)
{
    int wlen = 0;

#if TCFG_AUDIO_OUTPUT_IIS
    wlen = audio_mixer_ch_write(&dec->mix_ch, data, len);
#else
    s16 two_ch_data[MONO_TO_DUAL_POINTS * 2];
    s16 point_num = 0;
    u8 mono_to_dual = 0;
    s16 *mono_data = (s16 *)data;
    u16 remain_points = (len >> 1);
    /*
     *如果dac输出是双声道，因为esco解码出来时单声道
     *所以这里要根据dac通道确定是否做单变双
     */

#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_LR)
    mono_to_dual = 1;
#endif
    if (mono_to_dual) {
        do {
            point_num = MONO_TO_DUAL_POINTS;
            if (point_num >= remain_points) {
                point_num = remain_points;
            }
            audio_pcm_mono_to_dual(two_ch_data, mono_data, point_num);
            int tmp_len = audio_mixer_ch_write(&dec->mix_ch, two_ch_data, point_num << 2);
            wlen += tmp_len;
            remain_points -= (tmp_len >> 2);
            if (tmp_len < (point_num << 2)) {
                break;
            }
            mono_data += point_num;
        } while (remain_points);
        wlen >>= 1;
    } else {
        wlen = audio_mixer_ch_write(&dec->mix_ch, data, len);
    }
#endif

    return wlen;

}

/*
*********************************************************************
*                  ESCO Decode DL_NS filter
* Description: ESCO语音解码输出下行降噪滤镜
* Arguments  : data - 音频数据，len - 音频byte长度
* Return	 : 输出到后级的数据长度
* Note(s)    : 下行降噪通话接通前为直通，接通后进行NS RUN处理。
*********************************************************************
*/
static int esco_decoder_dl_ns_filter(struct esco_dec_hdl *dec, s16 *data, int len)
{
#if TCFG_ESCO_DL_NS_ENABLE
    int wlen = 0;
    int ret_len = len;

    if (dec->dl_ns_remain) {
        wlen = esco_decoder_mix_filter(dec, dec->dl_ns_out + (dec->dl_ns_offset / 2), dec->dl_ns_remain);
        dec->dl_ns_offset += wlen;
        dec->dl_ns_remain -= wlen;
        if (dec->dl_ns_remain) {
            return 0;
        }
    }

    if (get_call_status() == BT_CALL_ACTIVE) {
        /*接通的时候再开始做降噪*/
        int ns_out = esco_dl_ns_run(data, dec->dl_ns_out, len);
        //输入消耗完毕，没有输出
        if (ns_out == 0) {
            return len;
        }
        len = ns_out;
    } else {
        memcpy(dec->dl_ns_out, data, len);
    }
    wlen = esco_decoder_mix_filter(dec, dec->dl_ns_out, len);
    dec->dl_ns_offset = wlen;
    dec->dl_ns_remain = len - wlen;
    return ret_len;
#else
    return 0;
#endif/*TCFG_ESCO_DL_NS_ENABLE*/
}

/*
*********************************************************************
*                  ESCO output after syncts
* Description: esco同步滤镜后的数据流滤镜
* Arguments  : data - 音频数据，len - 音频byte长度
* Return	 : 输出到后级的数据长度
* Note(s)    : 音频同步后使用。
*********************************************************************
*/
static int esco_output_after_syncts_filter(void *priv, s16 *data, int len)
{
    struct esco_dec_hdl *dec = (struct esco_dec_hdl *)priv;
    int wlen = 0;

#if TCFG_ESCO_DL_NS_ENABLE
    wlen = esco_decoder_dl_ns_filter(dec, data, len);
#else
    wlen = esco_decoder_mix_filter(dec, data, len);
#endif

    return wlen;
}

/*
 *slience_frames结束前增加淡入处理，防止声音突变，听到一下的不适感
 * */
static void esco_fade_in(struct esco_dec_hdl *dec, s16 *data, int len)
{
    if (dec->fade_trigger) {
        int tmp = 0;
        s16 *p = data;
        u16 frames = (len >> 1) / dec->channels;
        for (int i = 0; i < frames; i++) {
            for (int j = 0; j < dec->channels; j++) {
                tmp = *p;
                *p++ = dec->fade_volume * tmp / DIGITAL_OdB_VOLUME;
            }
            dec->fade_volume += dec->fade_step;
            if (dec->fade_volume > DIGITAL_OdB_VOLUME) {
                dec->fade_volume = DIGITAL_OdB_VOLUME;
                break;
            }
        }
        if (dec->fade_volume == DIGITAL_OdB_VOLUME) {
            dec->fade_trigger = 0;
        }
    }
}

AT(.bt_decode.text.cache.L2.esco)
static int esco_dec_output_handler(struct audio_decoder *decoder, s16 *buf, int size, void *priv)
{
    int wlen = 0;
    struct esco_dec_hdl *dec = container_of(decoder, struct esco_dec_hdl, decoder);

    int ret_len = size;
    int len = size;
    short *data = buf;

    /*非上次残留数据,进行后处理*/
    if (!dec->remain) {

#if (defined(TCFG_PHONE_MESSAGE_ENABLE) && (TCFG_PHONE_MESSAGE_ENABLE))
        phone_message_call_api_esco_out_data(data, len);
#endif/*TCFG_PHONE_MESSAGE_ENABLE*/
        if (priv) {
            audio_plc_run(data, len, *(u8 *)priv);
        }

#if TCFG_DIG_PHASE_INVERTER_EN
        digital_phase_inverter_s16(data, len);
#endif/*TCFG_DIG_PHASE_INVERTER_EN*/

#if (SYS_VOL_TYPE == VOL_TYPE_DIGITAL)
        audio_digital_vol_run(esco_dec->dvol, data, len);
        u16 dvol_val = esco_dvol_tab[app_var.aec_dac_gain];
        for (u16 i = 0; i < len / 2; i++) {
            s32 tmp_data = data[i];
            if (tmp_data < 0) {
                tmp_data = -tmp_data;
                tmp_data = (tmp_data * dvol_val) >> 14;
                tmp_data = -tmp_data;
            } else {
                tmp_data = (tmp_data * dvol_val) >> 14;
            }
            data[i] = tmp_data;
        }
#endif/*VOL_TYPE_DIGITAL*/

#if TCFG_AUDIO_NOISE_GATE
        /*来电去电铃声不做处理*/
        if (get_call_status() == BT_CALL_ACTIVE) {
            audio_noise_gate_run(data, data, len);
        }
#endif/*TCFG_AUDIO_NOISE_GATE*/

#if TCFG_PHONE_EQ_ENABLE
        if (esco_eq) {
            audio_eq_run(esco_eq, data, len);

        }
#endif

        if (dec->slience_frames) {
            if (dec->slience_frames == 1) {
                int fade_time = 6;
                int sample_rate = dec->decoder.fmt.sample_rate;
                dec->fade_step = DIGITAL_OdB_VOLUME / ((fade_time * sample_rate) / 1000);
                dec->fade_trigger = 1;
                dec->fade_volume = 0;
            } else {
                dec->fade_trigger = 0;
                dec->fade_volume = 0;
                memset(data, 0x0, len);
            }
            dec->slience_frames--;
        }
        esco_fade_in(dec, data, len);

    }

#if AUDIO_CODEC_SUPPORT_SYNC
    if (dec->syncts) {
        wlen = audio_syncts_frame_filter(dec->syncts, data, len);
        if (wlen < len) {
            audio_syncts_trigger_resume(dec->syncts, (void *)decoder, audio_filter_resume_decoder);
        }
        goto ret_handle;
    }
#endif
    wlen = esco_output_after_syncts_filter(dec, data, len);

ret_handle:
    int remain_len = len - wlen;

    dec->remain = remain_len;
    return wlen;
}

static int esco_dec_post_handler(struct audio_decoder *decoder)
{

    return 0;
}

static const struct audio_dec_handler esco_dec_handler = {
    .dec_probe  = esco_dec_probe_handler,
    .dec_output = esco_dec_output_handler,
    .dec_post   = esco_dec_post_handler,
};

void esco_dec_release()
{
    audio_decoder_task_del_wait(&decode_task, &esco_dec->wait);
    free(esco_dec);
    esco_dec = NULL;
}

void esco_dec_close();

static void esco_dec_event_handler(struct audio_decoder *decoder, int argc, int *argv)
{
    switch (argv[0]) {
    case AUDIO_DEC_EVENT_END:
        puts("AUDIO_DEC_EVENT_END\n");
        esco_dec_close();
        break;
    }
}


static void esco_dec_set_output_channel(struct esco_dec_hdl *dec)
{
    u8 dac_connect_mode = audio_dac_get_channel(&dac_hdl);
}

u16 source_sr;
void set_source_sample_rate(u16 sample_rate)
{
    source_sr = sample_rate;
}

u16 get_source_sample_rate()
{
    if (bt_audio_is_running()) {
        return source_sr;
    }
    return 0;
}

#if (SYS_VOL_TYPE == VOL_TYPE_DIGITAL)
void esco_digital_vol_set(u8 vol)
{
    if (esco_dec) {
        audio_digital_vol_set(esco_dec->dvol, vol);
    }
}
#endif/*VOL_TYPE_DIGITAL*/

int esco_dec_dac_gain_set(u8 gain)
{
    app_var.aec_dac_gain = gain;
    if (esco_dec) {
        audio_dac_set_analog_vol(&dac_hdl, gain);
    }
    return 0;
}

static int esco_decoder_syncts_setup(struct esco_dec_hdl *dec)
{
    int err = 0;
#if AUDIO_CODEC_SUPPORT_SYNC
#define ESCO_DELAY_TIME     60
#define ESCO_RECOGNTION_TIME 220
    int delay_time = ESCO_DELAY_TIME;
    if (app_var.siri_stu == 1 || app_var.siri_stu == 2) {
        /*fix : Siri出sniff蓝牙数据到音频通路延迟过长，容易引入同步的问题*/
        delay_time = ESCO_RECOGNTION_TIME;
    }
    struct audio_syncts_params params = {0};
    int sample_rate = dec->decoder.fmt.sample_rate;

    params.nch = dec->decoder.fmt.channel;
#if defined(TCFG_AUDIO_OUTPUT_IIS) && TCFG_AUDIO_OUTPUT_IIS
    params.pcm_device = PCM_OUTSIDE_DAC;
    params.rout_sample_rate = TCFG_IIS_SAMPLE_RATE;
#else
    params.pcm_device = PCM_INSIDE_DAC;
    params.rout_sample_rate = sample_rate;
#endif
    params.network = AUDIO_NETWORK_BT2_1;
    params.rin_sample_rate = sample_rate;
    params.priv = dec;
    params.factor = TIME_US_FACTOR;
    params.output = (int (*)(void *, void *, int))esco_output_after_syncts_filter;

    bt_audio_sync_nettime_select(0);//0 - 主机，1 - tws, 2 - BLE
    u8 frame_clkn = dec->esco_len >= 60 ? 12 : 6;
    dec->ts_handle = esco_audio_timestamp_create(frame_clkn, ESCO_DELAY_TIME, TIME_US_FACTOR);
    dec->frame_time = frame_clkn;
    audio_syncts_open(&dec->syncts, &params);
    if (!err) {
        dec->mix_ch_event_params[0] = (u32)&dec->mix_ch;
        dec->mix_ch_event_params[1] = (u32)dec->syncts;
        audio_mixer_ch_set_event_handler(&dec->mix_ch, (void *)dec->mix_ch_event_params, audio_mix_ch_event_handler);
    }
    dec->ts_start = 0;
#endif
    return err;
}

static void esco_decoder_syncts_free(struct esco_dec_hdl *dec)
{
#if AUDIO_CODEC_SUPPORT_SYNC
    if (dec->ts_handle) {
        esco_audio_timestamp_close(dec->ts_handle);
        dec->ts_handle = NULL;
    }
    if (dec->syncts) {
        audio_syncts_close(dec->syncts);
        dec->syncts = NULL;
    }
#endif
}

int esco_ul_stream_open(u32 sample_rate, u16 esco_len, u32 codec_type)
{
    int err = 0;
    printf("esco_ul_open,sr = %d,len = %d,type = %x\n", sample_rate, esco_len, codec_type);
    if (esco_dec->start == 0) {
        printf("esco_ul_stream close ing,return\n");
        return 0;
    }
    if (esco_dec->ul_stream_open) {
        printf("esco_ul_stream_open now,return\n");
        return 0;
    }
    esco_dec->ul_stream_open = 1;
    err = audio_aec_init(sample_rate);
    if (err) {
        printf("audio_aec_init failed:%d", err);
        //goto __err3;
    }

    err = esco_enc_open(codec_type, esco_len);
    if (err) {
        printf("audio_enc_open failed:%d", err);
        //goto __err3;
    }
    return err;
}

int esco_ul_stream_close(void)
{
    printf("esco_ul_stream_close\n");
    if (esco_dec->ul_stream_open == 0) {
        printf("esco_ul_stream_closed,return\n");
        return 0;
    }
    audio_aec_close();
    esco_enc_close();
    esco_dec->ul_stream_open = 0;
    return 0;
}

int esco_ul_stream_switch(u8 en)
{
    if (esco_dec == NULL) {
        printf("esco_ul_stream_close now,return\n");
        return 0;
    }
    if (en) {
        return esco_ul_stream_open(esco_dec->sample_rate, esco_dec->esco_len, esco_dec->coding_type);
    } else {
        return esco_ul_stream_close();
    }
}

int esco_dec_start()
{
    int err;
    struct audio_fmt f;
    enum audio_channel channel;
    struct esco_dec_hdl *dec = esco_dec;
    u16 mix_buf_len_fix = 240;

    if (!esco_dec) {
        return -EINVAL;
    }

    err = audio_decoder_open(&dec->decoder, &esco_input, &decode_task);
    if (err) {
        goto __err1;
    }

    audio_decoder_set_handler(&dec->decoder, &esco_dec_handler);
    audio_decoder_set_event_handler(&dec->decoder, esco_dec_event_handler, 0);

    if (dec->coding_type == AUDIO_CODING_MSBC) {
        f.coding_type = AUDIO_CODING_MSBC;
        f.sample_rate = 16000;
        f.channel = 1;
    } else if (dec->coding_type == AUDIO_CODING_CVSD) {
        f.coding_type = AUDIO_CODING_CVSD;
        f.sample_rate = 8000;
        f.channel = 1;
        mix_buf_len_fix = 120;
    }

    set_source_sample_rate(f.sample_rate);
    esco_dec_set_output_channel(esco_dec);

    err = audio_decoder_set_fmt(&dec->decoder, &f);
    if (err) {
        goto __err2;
    }
    dec->channels = f.channel;

    /*
     *虽然mix有直通的处理，但是如果混合第二种声音进来的时候，就会按照mix_buff
     *的大小来混合输出，该buff太大，回导致dac没有连续的数据播放
     */
    /*audio_mixer_set_output_buf(&mixer, mix_buff, sizeof(mix_buff) / 8);*/
    audio_mixer_set_output_buf(&mixer, mix_buff, mix_buf_len_fix);
    audio_mixer_ch_open(&dec->mix_ch, &mixer);

#if defined(TCFG_AUDIO_OUTPUT_IIS) && TCFG_AUDIO_OUTPUT_IIS
    audio_mixer_ch_set_sample_rate(&dec->mix_ch, TCFG_IIS_SAMPLE_RATE);
#else
    audio_mixer_ch_set_sample_rate(&dec->mix_ch, f.sample_rate);
#endif
    audio_mixer_ch_set_resume_handler(&dec->mix_ch, (void *)&dec->decoder, (void (*)(void *))audio_decoder_resume);

    app_audio_state_switch(APP_AUDIO_STATE_CALL, app_var.aec_dac_gain);
#if (SYS_VOL_TYPE == VOL_TYPE_DIGITAL)
    esco_dec->dvol = audio_digital_vol_open(app_audio_get_volume(APP_AUDIO_STATE_CALL), 15, 4);
#endif/*VOL_TYPE_DIGITAL*/
    printf("max_vol:%d,call_vol:%d", app_var.aec_dac_gain, app_audio_get_volume(APP_AUDIO_STATE_CALL));
    app_audio_set_volume(APP_AUDIO_STATE_CALL, app_var.call_volume, 1);

    audio_plc_open(f.sample_rate);

#if TCFG_AUDIO_NOISE_GATE
    /*限幅器上限*/
#define LIMITER_THR	 -10000 /*-12000 = -12dB,放大1000倍,(-10000参考)*/
    /*小于CONST_NOISE_GATE的当成噪声处理,防止清0近端声音*/
#define LIMITER_NOISE_GATE  -40000 /*-12000 = -12dB,放大1000倍,(-30000参考)*/
    /*低于噪声门限阈值的增益 */
#define LIMITER_NOISE_GAIN  (0 << 30) /*(0~1)*2^30*/
    audio_noise_gate_open(f.sample_rate, LIMITER_THR, LIMITER_NOISE_GATE, LIMITER_NOISE_GAIN);
#endif/*TCFG_AUDIO_NOISE_GATE*/

#if TCFG_PHONE_EQ_ENABLE
    esco_eq = zalloc(sizeof(struct audio_eq) + sizeof(struct hw_eq_ch));
    if (esco_eq) {
        esco_eq->eq_ch = (struct hw_eq_ch *)((int)esco_eq + sizeof(struct audio_eq));
        struct audio_eq_param esco_eq_param;
        esco_eq_param.channels = f.channel;
        esco_eq_param.online_en = 1; // 支持在线调试
        esco_eq_param.mode_en = 0;
        esco_eq_param.remain_en = 0;
        esco_eq_param.max_nsection = EQ_SECTION_MAX;
        esco_eq_param.cb = eq_phone_get_filter_info;
        esco_eq_param.eq_name = AUDIO_CALL_EQ_NAME;
        audio_eq_open(esco_eq, &esco_eq_param);
        audio_eq_set_samplerate(esco_eq, f.sample_rate);
        audio_eq_set_output_handle(esco_eq, esco_eq_output, dec);
        audio_eq_start(esco_eq);
    }
#endif



#if AUDIO_CODEC_SUPPORT_SYNC
    esco_decoder_syncts_setup(dec);
#endif/*AUDIO_CODEC_SUPPORT_SYNC*/


    audio_dac_set_delay_time(&dac_hdl, 30, 50);
    lmp_private_esco_suspend_resume(2);
    err = audio_decoder_start(&dec->decoder);
    if (err) {
        goto __err3;
    }
    audio_out_effect_dis = 1;//通话模式关闭高低音

    dec->start = 1;
    dec->remain = 0;
    dec->sample_rate = f.sample_rate;

#if ESCO_DEC_MOVABLE_ENABLE
    int code_size = __esco_text_cache_L2_end - __esco_text_cache_L2_start;
    if (code_size && esco_code_run_addr == NULL) {
        esco_code_run_addr = phy_malloc(code_size);
        printf("---------load_code[esco]: %x, %x, %x, %x, %x\n", esco_code_run_addr,
               __esco_text_cache_L2_start, code_size,
               __esco_movable_slot_start, __esco_movable_slot_end);
        ASSERT(esco_code_run_addr != NULL);
        if (esco_code_run_addr) {
            code_movable_load(__esco_text_cache_L2_start, code_size, esco_code_run_addr,
                              __esco_movable_slot_start, __esco_movable_slot_end);
        }
    }
#endif/*ESCO_DEC_MOVABLE_ENABLE*/

#if TCFG_ESCO_DL_NS_ENABLE
    esco_dl_ns_open(dec, f.sample_rate);
#endif/*TCFG_ESCO_DL_NS_ENABLE*/

    /*打开上行通路数据流*/
#if (ESCO_AUTO_POWER_BALANCE_ROLE_SWITCH_ENABLE==1)&&(TCFG_USER_TWS_ENABLE==1)
    if (tws_api_get_role() == TWS_ROLE_SLAVE) {
        r_printf("<<<<<<<<<<<<<<<<<<esco_ul_stream_close");
    } else {
        r_printf("<<<<<<<<<<<<<<<<<<esco_ul_stream_open");
        esco_ul_stream_open(f.sample_rate, dec->esco_len, dec->coding_type);
    }

#else
    esco_ul_stream_open(f.sample_rate, dec->esco_len, dec->coding_type);
#endif
    audio_codec_clock_set(AUDIO_ESCO_MODE, dec->coding_type, dec->wait.preemption);
    dec->enc_start = 1; //该函数所在任务优先级低可能未open编码就开始解码，加入enc开始的标志防止解码过快输出
    printf("esco_dec_start ok\n");
    mem_stats();

    return 0;

__err3:
    audio_mixer_ch_close(&dec->mix_ch);
__err2:
    audio_decoder_close(&dec->decoder);
__err1:
    esco_dec_release();
    return err;
}

static int __esco_dec_res_close(void)
{
    if (!esco_dec->start) {
        return 0;
    }
    esco_dec->start = 0;
    esco_dec->enc_start = 0;
    esco_dec->preempt = 1;

    //audio_aec_close();
    //esco_enc_close();
    esco_ul_stream_close();

    app_audio_state_exit(APP_AUDIO_STATE_CALL);
    audio_decoder_close(&esco_dec->decoder);
    audio_mixer_ch_close(&esco_dec->mix_ch);
#if A2DP_RX_AND_AUDIO_DELAY
    audio_dac_set_delay_time(&dac_hdl, 20, AUDIO_DAC_DELAY_TIME);
#else
    audio_dac_set_delay_time(&dac_hdl, 30, AUDIO_DAC_DELAY_TIME);
#endif
#if AUDIO_CODEC_SUPPORT_SYNC
    esco_decoder_syncts_free(esco_dec);
    esco_dec->sync_step = 0;
    esco_dec->preempt_state = DEC_PREEMTED_BY_PRIORITY;
#endif
#if TCFG_PHONE_EQ_ENABLE
    if (esco_eq) {
        audio_eq_close(esco_eq);
        free(esco_eq);
        esco_eq = NULL;
    }
#endif
    audio_out_effect_dis = 0;

#if (SYS_VOL_TYPE == VOL_TYPE_DIGITAL)
    audio_digital_vol_close(esco_dec->dvol);
    esco_dec->dvol = NULL;
#endif

#if TCFG_ESCO_DL_NS_ENABLE
    esco_dl_ns_close();
#endif/*TCFG_ESCO_DL_NS_ENABLE*/

    audio_plc_close();
#if TCFG_AUDIO_NOISE_GATE
    audio_noise_gate_close();
#endif/*TCFG_AUDIO_NOISE_GATE*/

#if ESCO_DEC_MOVABLE_ENABLE
    if (esco_code_run_addr) {
        code_movable_unload(__esco_text_cache_L2_start,
                            __esco_movable_slot_start, __esco_movable_slot_end);
        phy_free(esco_code_run_addr);
        esco_code_run_addr = NULL;
        printf("----unload_code[esco]\n");
    }
#endif/*ESCO_DEC_MOVABLE_ENABLE*/

    audio_codec_clock_del(AUDIO_ESCO_MODE);
    return 0;
}
static int esco_wait_res_handler(struct audio_res_wait *wait, int event)
{
    int err = 0;

    printf("esco_wait_res_handler:%d", event);
    if (event == AUDIO_RES_GET) {
        err = esco_dec_start();
    } else if (event == AUDIO_RES_PUT) {
        err = __esco_dec_res_close();
        lmp_private_esco_suspend_resume(1);
    }

    return err;
}

int esco_dec_open(void *param, u8 mute)
{
    int err;
    struct esco_dec_hdl *dec;
    u32 esco_param = *(u32 *)param;
    int esco_len = esco_param >> 16;
    int codec_type = esco_param & 0x000000ff;

#if TCFG_AUDIO_ANC_ENABLE
#ifdef CONFIG_ANC_OVERLAY
    if (anc_train_open_query()) {
        printf("anc_train_open,suspend esco_dec\n");
        lmp_private_esco_suspend_resume(1);
        return 0;
    }
#endif/*CONFIG_ANC_OVERLAY*/
#endif /*TCFG_AUDIO_ANC_ENABLE*/

    printf("esco_dec_open, type=%d,len=%d\n", codec_type, esco_len);

    dec = zalloc(sizeof(*dec));
    if (!dec) {
        return -ENOMEM;
    }
    esco_dec = dec;

    dec->esco_len = esco_len;
    if (codec_type == 3) {
        dec->coding_type = AUDIO_CODING_MSBC;
    } else if (codec_type == 2) {
        dec->coding_type = AUDIO_CODING_CVSD;
    }

    dec->tws_mute_en = mute;

    dec->wait.priority = 2;
    dec->wait.preemption = 1;
    dec->wait.handler = esco_wait_res_handler;
    err = audio_decoder_task_add_wait(&decode_task, &dec->wait);
    if (esco_dec && esco_dec->start == 0) {
        lmp_private_esco_suspend_resume(1);
    }

#if AUDIO_OUTPUT_AUTOMUTE
    mix_out_automute_skip(1);
#endif

    return err;
}


void esco_dec_close()
{
    if (!esco_dec) {
        return;
    }

    __esco_dec_res_close();
    esco_dec_release();

#if AUDIO_OUTPUT_AUTOMUTE
    mix_out_automute_skip(0);
#endif

#if (defined(TCFG_PHONE_MESSAGE_ENABLE) && (TCFG_PHONE_MESSAGE_ENABLE))
    phone_message_call_api_stop();
#endif

    audio_mixer_set_output_buf(&mixer, mix_buff, sizeof(mix_buff));
    puts("esco_dec_close: exit\n");
}


//////////////////////////////////////////////////////////////////////////////
u8 bt_audio_is_running(void)
{
    return (a2dp_dec || esco_dec);
}
u8 bt_media_is_running(void)
{
    return a2dp_dec != NULL;
}
u8 bt_phone_dec_is_running()
{
    return esco_dec != NULL;
}

static void audio_dac_trim_init(int arg)
{
    struct audio_dac_trim dac_trim;

    audio_dac_do_trim(&dac_hdl, &dac_trim, 0);
    syscfg_write(CFG_DAC_TRIM_INFO, (void *)&dac_trim, sizeof(dac_trim));

    audio_dac_set_trim_value(&dac_hdl, &dac_trim);
}

extern u32 read_capless_DTB(void);
static u8 audio_dec_inited = 0;
/*音频配置实时跟踪，可以用来查看ADC/DAC增益，或者其他寄存器配置*/
static void audio_config_trace(void *priv)
{
    printf(">>Audio_Config_Trace:\n");
    audio_gain_dump();
    //audio_adda_dump();
    //mem_stats();
}

//////////////////////////////////////////////////////////////////////////////
int audio_dec_init()
{
    int err;

    printf("audio_dec_init\n");

#if TCFG_WAV_TONE_MIX_ENABLE
    wav_decoder_init();
#endif

    tone_play_init();
#if TCFG_KEY_TONE_EN	// 按键音初始化
    int error = audio_key_tone_init();
#endif


    /*os_sem_create(&dac_sem, 0);*/
    err = audio_decoder_task_create(&decode_task, "audio_dec");
#if (SYS_VOL_TYPE == VOL_TYPE_DIGITAL)
    audio_digital_vol_init();
#endif
    audio_dac_init(&dac_hdl, &dac_data);

#if TCFG_AUDIO_ANC_ENABLE
    audio_dac_anc_set(&dac_hdl, 1);
#endif
    u32 dacr32 = read_capless_DTB();

    audio_dac_set_capless_DTB(&dac_hdl, dacr32);
    audio_dac_set_buff(&dac_hdl, dac_buff, sizeof(dac_buff));
#if A2DP_RX_AND_AUDIO_DELAY
    audio_dac_set_delay_time(&dac_hdl, 10, AUDIO_DAC_DELAY_TIME);
#else
    audio_dac_set_delay_time(&dac_hdl, 10, AUDIO_DAC_DELAY_TIME);
#endif
    audio_dac_set_analog_vol(&dac_hdl, 0);

    request_irq(IRQ_AUDIO_IDX, 2, audio_irq_handler, 0);
    struct audio_dac_trim dac_trim = {0};
    int len = syscfg_read(CFG_DAC_TRIM_INFO, (void *)&dac_trim, sizeof(dac_trim));
    if ((len != sizeof(dac_trim)) || audio_dac_trim_value_check(&dac_trim)) {
#if 0 //DAC 异步trim
        int argv[8];
        argv[0] = (int)audio_dac_trim_init;
        argv[1] = 2;
        do {
            int err = os_taskq_post_type("audio_dec", Q_CALLBACK, 4, argv);
            if (err == OS_ERR_NONE) {
                break;
            }
            if (err != OS_Q_FULL) {
                break;
            }
            os_time_dly(2);
        } while (1);
#else
        audio_dac_trim_init(0);
#endif
    } else {
        audio_dac_set_trim_value(&dac_hdl, &dac_trim);
    }
#if TCFG_MC_BIAS_AUTO_ADJUST
    mic_trim_run();
#endif/*TCFG_MC_BIAS_AUTO_ADJUST*/
    audio_dac_set_fade_handler(&dac_hdl, NULL, audio_fade_in_fade_out);
#if AUDIO_CODEC_SUPPORT_SYNC
    audio_sync_resample_set_filt(dac_sync_filt, sizeof(dac_sync_filt));
#endif

    /*硬件SRC模块滤波器buffer设置，可根据最大使用数量设置整体buffer*/
    audio_src_base_filt_init(audio_src_hw_filt, sizeof(audio_src_hw_filt));


    audio_mixer_open(&mixer);
    audio_mixer_set_handler(&mixer, &mix_handler);
    audio_mixer_set_event_handler(&mixer, mixer_event_handler);
    audio_mixer_set_output_buf(&mixer, mix_buff, sizeof(mix_buff));

#if AUDIO_OUTPUT_AUTOMUTE
    mix_out_automute_open();
#endif  //#if AUDIO_OUTPUT_AUTOMUTE

#if TCFG_AUDIO_CONFIG_TRACE
    sys_timer_add(NULL, audio_config_trace, 3000);
#endif/*TCFG_AUDIO_CONFIG_TRACE*/

    audio_dec_inited = 1;
    return err;
}

static u8 audio_dec_init_complete()
{
    /*不支持Audio功能，返回idle*/
#if (defined TCFG_AUDIO_ENABLE && (TCFG_AUDIO_ENABLE == 0))
    return 1;
#endif/*TCFG_AUDIO_ENABLE*/
    if (!audio_dec_inited) {
        return 0;
    }

    return 1;
}
REGISTER_LP_TARGET(audio_dec_init_lp_target) = {
    .name = "audio_dec_init",
    .is_idle = audio_dec_init_complete,
};


/*----------------------------------------------------------------------------*/
/**@brief    环绕音效切换测试例子
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
#if AUDIO_SURROUND_CONFIG
void surround_switch_test(void *p)
{

    if (!p) {
        return;
    }

    static u8 cnt_type = 0;
    if (EFFECT_OFF == cnt_type) {
        //中途关开测试
        static u8 en = 0;
        en = !en;
        audio_surround_parm_update(p, cnt_type, (surround_update_parm *)en);
    } else {
        //音效切换测试
        audio_surround_parm_update(p, cnt_type, NULL);
    }
    printf("cnt_type 0x%x\n", cnt_type);
    if (++cnt_type > EFFECT_OFF) {
        cnt_type = EFFECT_3D_PANORAMA;
    }
}
#endif


/*----------------------------------------------------------------------------*/
/**@brief    虚拟低音参数更新例子
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/

void vbass_udate_parm_test(void *p)
{
    if (!p) {
        return;
    }
#if AUDIO_VBASS_CONFIG
    vbass_hdl *vbass = p;
    //
#if 0
    //虚拟低音增益调节例子
    static  u32 test_level  = 4096;
    if (vbass) {
        vbass_update_parm def_parm = {0};
        def_parm.bass_f = 300;
        test_level += 100;
        if (test_level > 16384) {
            test_level = 4096;
        }
        def_parm.level = test_level;
        audio_vbass_parm_update(vbass, VBASS_UPDATE_PARM, &def_parm);
    }
#endif
#if 0
    //开关虚拟低音例子
    static u8 en = 0;// 0关  1开
    en = !en;
    audio_vbass_parm_update(vbass, VBASS_SW, (void *)en);
#endif
#endif
}


#if AUDIO_SURROUND_CONFIG
/*
 *环绕音效开关控制
 * */
void audio_surround_voice(struct a2dp_dec_hdl *dec)
{
    if (dec && dec->surround) {
        surround_hdl *surround = dec->surround;
        u8 en = dec->surround_eff;
        if (en) {
            if (const_surround_en & BIT(2)) {
                surround_update_parm parm = {0};
                parm.surround_type = EFFECT_3D_LRDRIFT2;//音效类型
                parm.rotatestep    = 4;   //建议1~6,环绕速度
                parm.damping = 40;//混响音量（0~70）,空间感
                parm.feedback = 100;//干声音量(0~100),清晰度
                parm.roomsize = 128;//无效参数
                audio_surround_parm_update(surround, EFFECT_SUR2, &parm);
            } else {
                audio_surround_parm_update(surround, EFFECT_3D_ROTATES, NULL);
            }
        } else {
            surround_update_parm parm = {0};
            audio_surround_parm_update(surround, EFFECT_OFF2, &parm);//关音效
        }
    }
}

#if TCFG_USER_TWS_ENABLE

#define TWS_FUNC_ID_A2DP_EFF \
	((int)(('A' + '2' + 'D' + 'P') << (2 * 8)) | \
	 (int)(('E' + 'F' + 'F') << (1 * 8)) | \
	 (int)(('S' + 'Y' + 'N' + 'C') << (0 * 8)))
/*
 *发环绕左右耳效果同步
 * */
void audio_surround_voice_ctrl()
{
    int state = tws_api_get_tws_state();
    if (state & TWS_STA_SIBLING_CONNECTED) {
        if (a2dp_dec && a2dp_dec->surround) {
            if (!a2dp_dec->surround_eff) {
                a2dp_dec->surround_eff =  1;
            } else {
                a2dp_dec->surround_eff =  0;
            }
            int a2dp_eff = a2dp_dec->surround_eff;
            tws_api_send_data_to_sibling((u8 *)&a2dp_eff, sizeof(int), TWS_FUNC_ID_A2DP_EFF);
        }
    }
}
/*
 *左右耳环绕效果同步回调
 * */
static void tws_a2dp_eff_align(void *data, u16 len, bool rx)
{
    if (a2dp_dec && a2dp_dec->surround) {
        int a2dp_eff;
        memcpy(&a2dp_eff, data, sizeof(int));
        a2dp_dec->surround_eff = a2dp_eff;
        a2dp_surround_eff = a2dp_eff;
        audio_surround_voice(a2dp_dec);
    }
}

REGISTER_TWS_FUNC_STUB(a2dp_align_eff) = {
    .func_id = TWS_FUNC_ID_A2DP_EFF,
    .func    = tws_a2dp_eff_align,
};
#endif /* TCFG_USER_TWS_ENABLE */

#endif /* AUDIO_SURROUND_CONFIG */



#if TCFG_AUDIO_BASS_BOOST_TEST
#define TWS_FUNC_ID_BASS_EFF \
	((int)(('A' + '2' + 'D' + 'P') << (2 * 8)) | \
	 (int)(('E' + 'F' + 'F') << (1 * 8)) | \
	 (int)(('S' + 'Y' + 'N' + 'C') << (0 * 8)))
void audio_bass_ctrl()
{
    int state = tws_api_get_tws_state();
    if (state & TWS_STA_SIBLING_CONNECTED) {
        if (a2dp_dec) {
            test_wdrctmp = !test_wdrctmp;
            if (test_wdrctmp) { //对耳链接，进入低音增强，播个提示音
                tone_play_index(TONE_NORMAL, 1);
            } else {
            }

            tws_api_send_data_to_sibling((u8 *)&test_wdrctmp, sizeof(int), TWS_FUNC_ID_BASS_EFF);
        }
    }
}

static void tws_a2dp_eff_align_bass(void *data, u16 len, bool rx)
{
    if (a2dp_dec) {
        int a2dp_eff;
        memcpy(&a2dp_eff, data, sizeof(int));
        wdrc_run = a2dp_eff;
        test_wdrctmp = wdrc_run;
    }
}

REGISTER_TWS_FUNC_STUB(a2dp_align_bass) = {
    .func_id = TWS_FUNC_ID_BASS_EFF,
    .func    = tws_a2dp_eff_align_bass,
};
#endif/*TCFG_AUDIO_BASS_BOOST_TEST*/


/*----------------------------------------------------------------------------*/
/**@brief    自定义压缩器系数回调
   @param    *drc: 句柄
   @param    *info: 系数结构地址
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
int wdrc_get_filter_info_demo(struct audio_drc_filter_info *info)
{
#if (TCFG_AUDIO_BASS_BOOST && TCFG_DRC_ENABLE)
    static struct drc_ch wdrc_p = {0};
    struct threshold_group low_threshold[] = {//低频
        {0, 0},
        {79, 79},
        {140, 80},
    };

    struct threshold_group mid_threshold[] = {//中频
        {0, 0},
        {82, 82},
        {140, 82}
    };

    wdrc_p.nband = 2;
    wdrc_p.type = 3;//wdrc
    wdrc_p.low_freq = 90; //低中分频点  （20~600hz）
    wdrc_p.high_freq = 2000;//中高分频点   (0~20000hz)
    wdrc_p.order = 2;////分频阶数 2或者4
    int i = 0;
    //低频
    //left
    wdrc_p._p.wdrc[i][0].attacktime = 10;
    wdrc_p._p.wdrc[i][0].releasetime = 100;
    memcpy(wdrc_p._p.wdrc[i][0].threshold, low_threshold, sizeof(low_threshold));
    wdrc_p._p.wdrc[i][0].threshold_num = ARRAY_SIZE(low_threshold);
    wdrc_p._p.wdrc[i][0].rms_time = 25;
    wdrc_p._p.wdrc[i][0].algorithm = 0;
    wdrc_p._p.wdrc[i][0].mode = 1;
    wdrc_p._p.wdrc[i][0].inputgain = 0;
    wdrc_p._p.wdrc[i][0].outputgain = 8;
    //right
    wdrc_p._p.wdrc[i][1].attacktime = 10;
    wdrc_p._p.wdrc[i][1].releasetime = 100;
    memcpy(wdrc_p._p.wdrc[i][1].threshold, low_threshold, sizeof(low_threshold));
    wdrc_p._p.wdrc[i][1].threshold_num = ARRAY_SIZE(low_threshold);
    wdrc_p._p.wdrc[i][1].rms_time = 25;
    wdrc_p._p.wdrc[i][1].algorithm = wdrc_p._p.wdrc[i][0].algorithm;
    wdrc_p._p.wdrc[i][1].mode = wdrc_p._p.wdrc[i][0].mode;
    wdrc_p._p.wdrc[i][1].inputgain = wdrc_p._p.wdrc[i][0].inputgain;
    wdrc_p._p.wdrc[i][1].outputgain = wdrc_p._p.wdrc[i][0].outputgain;

    //高频
    i = 1;
    //left
    wdrc_p._p.wdrc[i][0].attacktime = 10;
    wdrc_p._p.wdrc[i][0].releasetime = 100;
    memcpy(wdrc_p._p.wdrc[i][0].threshold, mid_threshold, sizeof(mid_threshold));
    wdrc_p._p.wdrc[i][0].threshold_num = ARRAY_SIZE(mid_threshold);
    wdrc_p._p.wdrc[i][0].rms_time = 25;
    wdrc_p._p.wdrc[i][0].algorithm = 0;
    wdrc_p._p.wdrc[i][0].mode = 1;
    wdrc_p._p.wdrc[i][0].inputgain = 0;
    wdrc_p._p.wdrc[i][0].outputgain = 0;

    //right
    wdrc_p._p.wdrc[i][1].attacktime = 10;
    wdrc_p._p.wdrc[i][1].releasetime = 100;
    memcpy(wdrc_p._p.wdrc[i][1].threshold, mid_threshold, sizeof(mid_threshold));
    wdrc_p._p.wdrc[i][1].threshold_num = ARRAY_SIZE(mid_threshold);
    wdrc_p._p.wdrc[i][1].rms_time = 25;
    wdrc_p._p.wdrc[i][1].algorithm = wdrc_p._p.wdrc[i][0].algorithm;
    wdrc_p._p.wdrc[i][1].mode = wdrc_p._p.wdrc[i][0].mode;
    wdrc_p._p.wdrc[i][1].inputgain = wdrc_p._p.wdrc[i][0].inputgain;
    wdrc_p._p.wdrc[i][1].outputgain = wdrc_p._p.wdrc[i][0].outputgain;


    info->R_pch = info->pch = &wdrc_p;
#endif/*TCFG_AUDIO_BASS_BOOST*/
    return 0;
}

void a2dp_decoder_wdrc_filter_setup(struct a2dp_dec_hdl *dec, struct audio_fmt *fmt)
{
#if (TCFG_AUDIO_BASS_BOOST && TCFG_DRC_ENABLE)
    a2dp_wdrc = malloc(sizeof(struct audio_drc));
    if (a2dp_wdrc) {
        struct audio_drc_param drc_param = {0};
        drc_param.channels = dec->ch;
        drc_param.online_en = 0;
        drc_param.remain_en = 0;
        drc_param.cb = wdrc_get_filter_info_demo;
        audio_drc_open(a2dp_wdrc, &drc_param);
        audio_drc_set_samplerate(a2dp_wdrc, fmt->sample_rate);
        audio_drc_set_output_handle(a2dp_wdrc, NULL, NULL);
        audio_drc_start(a2dp_wdrc);
    }
#endif/*TCFG_AUDIO_BASS_BOOST*/
}





