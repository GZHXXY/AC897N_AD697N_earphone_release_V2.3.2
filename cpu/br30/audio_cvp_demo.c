/*
 ****************************************************************
 *							AUDIO_CVP_DEMO
 * File  : audio_cvp_demo.c
 * By    :
 * Notes :清晰语音测试使用，请不要修改本demo，如有需求，请拷贝副
 *		  本，自行修改!
 * Usage :
 *(1)测试单mic降噪和双mic降噪
 *(2)支持监听：监听原始数据/处理后的数据(MONITOR)
 *	MIC ---------> CVP ---------->SPK
 *			|				|
 *	   monitor probe	monitor post
 *(3)调用audio_cvp_test()即可测试cvp模块
 ****************************************************************
 */
#include "asm/includes.h"
#include "media/includes.h"
#include "system/includes.h"
#include "aec_user.h"
#include "app_main.h"
#include "audio_config.h"
#include "audio_enc.h"

//#define CVP_LOG_ENABLE
#ifdef CVP_LOG_ENABLE
#define CVP_LOG(x, ...)  y_printf("[cvp_demo]" x " ", ## __VA_ARGS__)
#else
#define CVP_LOG(...)
#endif/*CVP_LOG_ENABLE*/

extern struct adc_platform_data adc_data;
extern struct audio_dac_hdl dac_hdl;
extern struct audio_adc_hdl adc_hdl;

#if TCFG_AUDIO_DUAL_MIC_ENABLE
#define MIC_CH_NUM			2
#else
#define MIC_CH_NUM			1
#endif/*TCFG_AUDIO_DUAL_MIC_ENABLE*/
#define MIC_BUF_NUM        	2
#define MIC_IRQ_POINTS     	256
#define MIC_BUFS_SIZE      	(MIC_BUF_NUM * MIC_IRQ_POINTS * MIC_CH_NUM)

#define CVP_MIC_SR			16000	//采样率
#define CVP_MIC_GAIN		10		//增益

#define CVP_MONITOR_DIS		0		//监听关闭
#define CVP_MONITOR_PROBE	1		//监听处理前数据
#define CVP_MONITOR_POST	2		//监听处理后数据
//默认监听选择
#define CVP_MONITOR_SEL		CVP_MONITOR_POST

typedef struct {
    u8 monitor;
    struct audio_adc_output_hdl adc_output;
    struct adc_mic_ch mic_ch;
    s16 tmp_buf[MIC_IRQ_POINTS];
    s16 mic_buf[MIC_BUFS_SIZE];
} audio_cvp_t;
audio_cvp_t *cvp_demo = NULL;

/*监听输出：默认输出到dac*/
static int cvp_monitor_output(s16 *data, int len)
{
    int wlen = audio_dac_write(&dac_hdl, data, len);
    if (wlen != len) {
        CVP_LOG("monitor output full\n");
    }
    return wlen;
}

/*监听使能*/
static int cvp_monitor_en(u8 en, int sr)
{
    CVP_LOG("cvp_monitor_en:%d,sr:%d\n", en, sr);
    if (en) {
        app_audio_state_switch(APP_AUDIO_STATE_MUSIC, get_max_sys_vol());
        CVP_LOG("cur_vol:%d,max_sys_vol:%d\n", app_audio_get_volume(APP_AUDIO_STATE_MUSIC), get_max_sys_vol());
        audio_dac_set_volume(&dac_hdl, app_audio_get_volume(APP_AUDIO_STATE_MUSIC));
        audio_dac_set_sample_rate(&dac_hdl, sr);
        audio_dac_start(&dac_hdl);
    } else {
        audio_dac_stop(&dac_hdl);
    }
    return 0;
}

/*mic adc原始数据输出*/
static void mic_output(void *priv, s16 *data, int len)
{
    putchar('.');
    int wlen = 0;

#if (MIC_CH_NUM == 2)/*DualMic*/
    s16 *mic0_data = data;
    s16 *mic1_data = cvp_demo->tmp_buf;
    s16 *mic1_data_pos = data + (len / 2);
    //printf("mic_data:%x,%x,%d\n",data,mic1_data_pos,len);
    /*分离出两个mic通道的数据*/
    for (u16 i = 0; i < (len >> 1); i++) {
        mic0_data[i] = data[i * 2];
        mic1_data[i] = data[i * 2 + 1];
    }
    memcpy(mic1_data_pos, mic1_data, len);

    if (cvp_demo->monitor == CVP_MONITOR_PROBE) {
        cvp_monitor_output(data, len);
    }

#if 0 /*debug*/
    static u16 mic_cnt = 0;
    if (mic_cnt++ > 300) {
        putchar('1');
        audio_aec_inbuf(mic1_data_pos, len);
        if (mic_cnt > 600) {
            mic_cnt = 0;
        }
    } else {
        putchar('0');
        audio_aec_inbuf(mic0_data, len);
    }
#else
#if (TCFG_AUDIO_DMS_MIC_MANAGE == DMS_MASTER_MIC0)
    audio_aec_inbuf_ref(mic1_data_pos, len);
    audio_aec_inbuf(data, len);
#else
    audio_aec_inbuf_ref(data, len);
    audio_aec_inbuf(mic1_data_pos, len);
#endif/*TCFG_AUDIO_DMS_MIC_MANAGE*/
#endif/*debug end*/
#else /*SingleMic*/
    if (cvp_demo->monitor == CVP_MONITOR_PROBE) {
        cvp_monitor_output(data, len);
    }
    audio_aec_inbuf(data, len);
#endif/*TCFG_AUDIO_DUAL_MIC_ENABLE*/
}

static void mic_open(u8 mic0_en, u8 mic1_en, u8 gain0, u8 gain1, int sr)
{
    CVP_LOG("mic_open\n");
    if (cvp_demo) {
        if (mic0_en) {
            audio_adc_mic_open(&cvp_demo->mic_ch, AUDIO_ADC_MIC_CH, &adc_hdl);
            audio_adc_mic_set_gain(&cvp_demo->mic_ch, gain0);
        }
        if (mic1_en) {
            audio_adc_mic1_open(&cvp_demo->mic_ch, AUDIO_ADC_MIC_CH, &adc_hdl);
            audio_adc_mic1_set_gain(&cvp_demo->mic_ch, gain0);
        }
        audio_adc_mic_set_sample_rate(&cvp_demo->mic_ch, sr);
        audio_adc_mic_set_buffs(&cvp_demo->mic_ch, cvp_demo->mic_buf, MIC_IRQ_POINTS * 2, MIC_BUF_NUM);
        cvp_demo->adc_output.handler = mic_output;
        audio_adc_add_output_handler(&adc_hdl, &cvp_demo->adc_output);
        audio_adc_mic_start(&cvp_demo->mic_ch);
    }
    CVP_LOG("mic_open succ\n");
}

static void mic_close(void)
{
    if (cvp_demo) {
        audio_adc_mic_close(&cvp_demo->mic_ch);
        audio_adc_del_output_handler(&adc_hdl, &cvp_demo->adc_output);
    }
}

/*清晰语音数据输出*/
static int cvp_output_hdl(s16 *data, u16 len)
{
    putchar('o');
    if (cvp_demo->monitor == CVP_MONITOR_POST) {
        cvp_monitor_output(data, len);
    }
    return len;
}

/*清晰语音测试模块*/
int audio_cvp_test(void)
{
    if (cvp_demo == NULL) {
        cvp_demo = zalloc(sizeof(audio_cvp_t));
        ASSERT(cvp_demo);
        cvp_demo->monitor = CVP_MONITOR_SEL;
        if (cvp_demo->monitor) {
            cvp_monitor_en(1, CVP_MIC_SR);
        }
#if TCFG_AUDIO_DUAL_MIC_ENABLE
        mic_open(1, 1, CVP_MIC_GAIN, CVP_MIC_GAIN, CVP_MIC_SR);
        audio_aec_open(CVP_MIC_SR, (ANS_EN | ENC_EN), cvp_output_hdl);
#else
        mic_open(1, 0, CVP_MIC_GAIN, CVP_MIC_GAIN, CVP_MIC_SR);
        audio_aec_open(CVP_MIC_SR, (ANS_EN), cvp_output_hdl);
#endif/*TCFG_AUDIO_DUAL_MIC_ENABLE*/
        CVP_LOG("cvp_test open\n");
    } else {
        mic_close();
        if (cvp_demo->monitor) {
            cvp_monitor_en(0, CVP_MIC_SR);
        }
        audio_aec_close();
        free(cvp_demo);
        cvp_demo = NULL;
        CVP_LOG("cvp_test close\n");
    }
    return 0;
}

static u8 audio_cvp_idle_query()
{
    return (cvp_demo == NULL) ? 1 : 0;
}
REGISTER_LP_TARGET(audio_cvp_lp_target) = {
    .name = "audio_cvp",
    .is_idle = audio_cvp_idle_query,
};
