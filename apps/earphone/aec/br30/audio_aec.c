/*
 ****************************************************************
 *							AUDIO SMS(SingleMic System)
 * File  : audio_aec_sms.c
 * By    :
 * Notes : AEC回音消除 + 单mic降噪
 *
 ****************************************************************
 */
#include "aec_user.h"
#include "system/includes.h"
#include "media/includes.h"
#include "application/audio_eq.h"
#include "application/audio_pitch.h"
#include "circular_buf.h"
#include "audio_aec_online.h"
#include "amplitude_statistic.h"
#include "audio_gain_process.h"

#if TCFG_AUDIO_DMS_DUT_ENABLE
#include "audio_dms_tool.h"
#endif /*TCFG_AUDIO_DMS_DUT_ENABLE*/

#if TCFG_USER_TWS_ENABLE
#include "bt_tws.h"
#endif/* TCFG_USER_TWS_ENABLE */

#if !defined(TCFG_CVP_DEVELOP_ENABLE) || (TCFG_CVP_DEVELOP_ENABLE == 0)

#if (TCFG_AUDIO_DUAL_MIC_ENABLE == 0)
#include "commproc.h"

#define LOG_TAG_CONST       AEC_USER
#define LOG_TAG             "[AEC_USER]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"
#include "audio_aec_debug.c"

/*AAC内存overlay的时候，AEC用户空间内存也overlay，提高利用率*/
#if TCFG_AUDIO_AAC_RAM_MALLOC_ENABLE
#define AEC_USER_MALLOC_ENABLE	1
#else
#define AEC_USER_MALLOC_ENABLE	1
#endif/*TCFG_AUDIO_AAC_RAM_MALLOC_ENABLE*/

/*AEC_TOGGLE:AEC模块使能开关，Disable则数据完全不经过处理，AEC模块不占用资源*/
#if TCFG_AUDIO_INPUT_IIS
#define AEC_TOGGLE				0
#else
#define AEC_TOGGLE				1
#endif
/*mic去直流滤波eq*/
#define AEC_DCCS_EN				TCFG_AEC_DCCS_EQ_ENABLE
/*mic普通eq*/
#define AEC_UL_EQ_EN			TCFG_AEC_UL_EQ_ENABLE
extern const int  VC_KINDO_TVM;
extern struct adc_platform_data adc_data;
extern struct audio_dac_hdl dac_hdl;

#ifdef CONFIG_FPGA_ENABLE
const u8 CONST_AEC_ENABLE = 1;
#else
const u8 CONST_AEC_ENABLE = 1;
#endif/*CONFIG_FPGA_ENABLE*/

#ifdef AUDIO_PCM_DEBUG
/*AEC串口数据导出*/
const u8 CONST_AEC_EXPORT = 1;
#else
const u8 CONST_AEC_EXPORT = 0;
#endif/*AUDIO_PCM_DEBUG*/

//*********************************************************************************//
//                                预处理配置(Pre-process Config)               	   //
//*********************************************************************************//
/*预增益配置*/
#define CVP_PRE_GAIN_ENABLE				0	//算法处理前预加数字增益放大

/*响度指示器*/
#define CVP_LOUDNESS_TRACE_ENABLE		0	//跟踪获取当前mic输入幅值

//*********************************************************************************//
//                                调试配置(Debug Config)                           //
//*********************************************************************************//
/*使能即可跟踪通话过程的内存情况*/
#define CVP_MEM_TRACE_ENABLE	0

//#define TEST_ZALLOC_MUX_RX_BULK
#ifdef TEST_ZALLOC_MUX_RX_BULK
extern void *zalloc_mux_rx_bulk(int size);
extern void free_mux_rx_bulk(void *buf);
u32 *test_mux_rx_bulk0 = NULL;
u32 *test_mux_rx_bulk1 = NULL;
u32 *test_mux_rx_bulk2 = NULL;
#endif
/*
 *延时估计使能
 *点烟器需要做延时估计
 *其他的暂时不需要做
 */
const u8 CONST_AEC_DLY_EST = 0;

/*ANS等级:0 or 1,等级1比等级0多6k左右的ram*/
const u8 CONST_ANS_MODE = 1;

/*
 *ANS版本配置
 *ANS_V100:传统降噪
 *ANS_V200:AI降噪，需要更多的ram和mips
 **/
#if (TCFG_AUDIO_CVP_NS_MODE == CVP_ANS_MODE)
const u8 CONST_ANS_VERSION = ANS_V100;
#else
const u8 CONST_ANS_VERSION = ANS_V200;
#endif/*TCFG_AUDIO_CVP_NS_MODE*/

//*********************************************************************************//
//                                DNS配置                                          //
//*********************************************************************************//
/* SNR估计,可以实现场景适配 */
const u8 CONST_DNS_SNR_EST = 0;

/* DNS后处理 */
const u8 CONST_DNS_POST_ENHANCE = 0;

//*******************************DNS配置 end**************************************//

/*Splittingfilter模式：0 or 1
 *mode = 0:运算量和ram小，高频会跌下来
 *mode = 1:运算量和ram大，频响正常（过认证，选择模式1）
 */
const u8 CONST_SPLIT_FILTER_MODE = 0;

#if TCFG_AEC_SIMPLEX
/*限幅器-噪声门限*/
const u8 CONST_AEC_SIMPLEX = 1;
#else
const u8 CONST_AEC_SIMPLEX = 0;
#endif/*TCFG_AEC_SIMPLEX*/
/*单工连续清0的帧数*/
#define AEC_SIMPLEX_TAIL 	15
/**远端数据大于CONST_AEC_SIMPLEX_THR,即清零近端数据
 *越小，回音限制得越好，同时也就越容易卡*/
#define AEC_SIMPLEX_THR		100000	/*default:260000*/

/*数据输出开头丢掉的数据包数*/
#define AEC_OUT_DUMP_PACKET		15
/*数据输出开头丢掉的数据包数*/
#define AEC_IN_DUMP_PACKET		1

/*复用lmp rx buf(一般通话的时候复用)
 *rx_buf概率产生碎片，导致alloc失败，因此默认配0
 */
#define MALLOC_MULTIPLEX_EN		0
extern void *lmp_malloc(int);
extern void lmp_free(void *);
void *zalloc_mux(int size)
{
#if MALLOC_MULTIPLEX_EN
    void *p = NULL;
    do {
        p = lmp_malloc(size);
        if (p) {
            break;
        }
        printf("aec_malloc wait...\n");
        os_time_dly(2);
    } while (1);
    if (p) {
        memset(p, 0, size);
    }
    printf("[malloc_mux]p = 0x%x,size = %d\n", p, size);
    return p;
#else
    return zalloc(size);
#endif
}

void free_mux(void *p)
{
#if MALLOC_MULTIPLEX_EN
    printf("[free_mux]p = 0x%x\n", p);
    lmp_free(p);
#else
    free(p);
#endif
}

struct audio_aec_hdl {
    u8 start;				//aec模块状态
    u8 inbuf_clear_cnt;		//aec输入数据丢掉
    u8 output_fade_in;		//aec输出淡入使能
    u8 output_fade_in_gain;	//aec输出淡入增益
    u8 EnableBit;			//aec使能模块
    u8 input_clear;			//清0输入数据标志
    u16 dump_packet;		//前面如果有杂音，丢掉几包
    u8 output_buf[1000];	//aec数据输出缓存
    cbuffer_t output_cbuf;
#if AEC_UL_EQ_EN
    struct audio_eq ul_eq;
    struct hw_eq_ch ul_eq_ch;
#endif/*AEC_UL_EQ_EN*/
#if AEC_DCCS_EN
    struct audio_eq dccs_eq;
    struct hw_eq_ch dccs_eq_ch;
#endif/*AEC_DCCS_EN*/
    struct aec_s_attr attr;	//aec模块参数属性
#if AEC_PITCHSHIFTER_CONFIG
    s_pitch_hdl *pitch ; //变声
    u8 pitch_mode_L; //记录变声模式
    u8 pitch_mode_R; //记录变声模式
#endif/* AEC_PITCHSHIFTER_CONFIG */
};
#if AEC_USER_MALLOC_ENABLE
struct audio_aec_hdl *aec_hdl = NULL;
#else
struct audio_aec_hdl aec_handle AT(.aec_mux);
struct audio_aec_hdl *aec_hdl = &aec_handle;
#endif/*AEC_USER_MALLOC_ENABLE*/

#if AEC_DCCS_EN
const int DCCS_8k_Coeff[5] = {
    (943718 << 2),	-(856687 << 2),	(1048576 << 2),	(1887437 << 2),	-(2097152 << 2)
};
const int DCCS_16k_Coeff[5] = {
    (1006633 << 2),	-(967542 << 2),	(1048576 << 2),	(2013266 << 2),	-(2097152 << 2)
};
int aec_dccs_eq_filter(int sr, struct audio_eq_filter_info *info)
{
    //r_printf("dccs_eq sr:%d\n", sr);
    if (sr == 16000) {
        info->L_coeff = (void *)DCCS_16k_Coeff;
        info->R_coeff = (void *)DCCS_16k_Coeff;
    } else {
        info->L_coeff = (void *)DCCS_8k_Coeff;
        info->R_coeff = (void *)DCCS_8k_Coeff;
    }
    info->L_gain = 0;
    info->R_gain = 0;
    info->nsection = 1;
    return 0;
}

static int dccs_eq_output(void *priv, void *data, u32 len)
{
    return 0;
}
#endif/*AEC_DCCS_EN*/

#if AEC_UL_EQ_EN
static int ul_eq_output(void *priv, void *data, u32 len)
{
    return 0;
}
#endif/*AEC_UL_EQ_EN*/

/*
*********************************************************************
*                  Audio AEC Process_Probe
* Description: AEC模块数据前处理回调(预处理)
* Arguments  : mic0 mic0数据地址
*			   mic1	mic1数据地址
*			   mic2	mic2数据地址
*			   ref	参考数据地址
*			   len	数据长度(Unit:byte)
* Return	 : 0 成功 其他 失败
* Note(s)    : 在源数据经过CVP模块前，可以增加自定义处理
*********************************************************************
*/
static LOUDNESS_M_STRUCT mic_loudness;
static int audio_aec_probe(short *mic0, short *mic1, short *mic2, short *ref, u16 len)
{
#if CVP_PRE_GAIN_ENABLE
    GainProcess_16Bit(mic0, mic0, 12.f, 1, 1, 1, len >> 1);
#endif/*CVP_PRE_GAIN_ENABLE*/

#if CVP_LOUDNESS_TRACE_ENABLE
    loudness_meter_short(&mic_loudness, mic0, len >> 1);
#endif/*CVP_LOUDNESS_TRACE_ENABLE*/

#if AEC_DCCS_EN
    if (aec_hdl->dccs_eq.start) {
        audio_eq_run(&aec_hdl->dccs_eq, mic0, len);
    }
#endif/*AEC_DCCS_EN*/
    return 0;
}

/*
*********************************************************************
*                  Audio AEC Process_Post
* Description: AEC模块数据后处理回调
* Arguments  : data 数据地址
*			   len	数据长度
* Return	 : 0 成功 其他 失败
* Note(s)    : 在数据处理完毕，可以增加自定义后处理
*********************************************************************
*/
static int audio_aec_post(s16 *data, u16 len)
{
#if AEC_UL_EQ_EN
    if (aec_hdl->ul_eq.start) {
        audio_eq_run(&aec_hdl->ul_eq, data, len);
    }
#endif/*AEC_UL_EQ_EN*/

#if AEC_PITCHSHIFTER_CONFIG
    if (aec_hdl && aec_hdl->pitch) {
        pitch_run(aec_hdl->pitch, data, data, len, 1);
    }
#endif/* AEC_PITCHSHIFTER_CONFIG */
    return 0;
}

int audio_aec_update(u8 EnableBit)
{
#if (TCFG_KWS_VOICE_RECOGNITION_ENABLE == 1)
    extern u8 kws_is_running(void);
    if (kws_is_running()) { /*当是KWS调用的，使用默认参数的EnableBit恢复clk*/
        EnableBit = aec_hdl->attr.EnableBit;
    }
#endif /*TCFG_KWS_VOICE_RECOGNITION_ENABLE*/
    printf("aec_update,wideband:%d,EnableBit:%x", aec_hdl->attr.wideband, EnableBit);
    if (CONST_ANS_VERSION == ANS_V200) {
        clk_set("sys", CONFIG_BT_CALL_DNS_HZ);
        return 0;
    }
    if (aec_hdl->attr.wideband) {
        if ((EnableBit & 0x7) == AEC_MODE_ADVANCE) {
            /*clk_set("sys", BT_CALL_16k_ADVANCE_HZ);*/
            clk_set_sys_lock(BT_CALL_16k_ADVANCE_HZ, 1);
        } else {
            /*clk_set("sys", BT_CALL_16k_HZ);*/
            clk_set_sys_lock(BT_CALL_16k_HZ, 1);
        }
    } else {
        if ((EnableBit & 0x7) == AEC_MODE_ADVANCE) {
            /*clk_set("sys", BT_CALL_ADVANCE_HZ);*/
            clk_set_sys_lock(BT_CALL_ADVANCE_HZ, 1);
        } else {
            /*clk_set("sys", BT_CALL_HZ);*/
            clk_set_sys_lock(BT_CALL_HZ, 1);
        }
    }
    return 0;
}

/*跟踪系统内存使用情况:physics memory size xxxx bytes*/
static void sys_memory_trace(void)
{
    static int cnt = 0;
    if (cnt++ > 200) {
        cnt = 0;
        mem_stats();
    }
}
/*
*********************************************************************
*                  Audio AEC Output Handle
* Description: AEC模块数据输出回调
* Arguments  : data 输出数据地址
*			   len	输出数据长度
* Return	 : 数据输出消耗长度
* Note(s)    : None.
*********************************************************************
*/
extern void esco_enc_resume(void);
static int audio_aec_output(s16 *data, u16 len)
{
#if TCFG_KWS_VOICE_RECOGNITION_ENABLE
    //Voice Recognition get mic data here
    extern void kws_aec_data_output(void *priv, s16 * data, int len);
    kws_aec_data_output(NULL, data, len);

#endif/*TCFG_KWS_VOICE_RECOGNITION_ENABLE*/

#if CVP_MEM_TRACE_ENABLE
    sys_memory_trace();
#endif/*CVP_MEM_TRACE_ENABLE*/

    if (aec_hdl->dump_packet) {
        aec_hdl->dump_packet--;
        memset(data, 0, len);
    } else  {
        if (aec_hdl->output_fade_in) {
            s32 tmp_data;
            //printf("fade:%d\n",aec_hdl->output_fade_in_gain);
            for (int i = 0; i < len / 2; i++) {
                tmp_data = data[i];
                data[i] = tmp_data * aec_hdl->output_fade_in_gain >> 7;
            }
            aec_hdl->output_fade_in_gain += 12;
            if (aec_hdl->output_fade_in_gain >= 128) {
                aec_hdl->output_fade_in = 0;
            }
        }
    }
    u16 wlen = cbuf_write(&aec_hdl->output_cbuf, data, len);
    //printf("wlen:%d-%d\n",len,aec_hdl.output_cbuf.data_len);
    esco_enc_resume();
#if 1
    static u32 aec_output_max = 0;
    if (aec_output_max < aec_hdl->output_cbuf.data_len) {
        aec_output_max = aec_hdl->output_cbuf.data_len;
        y_printf("o_max:%d", aec_output_max);
    }
#endif
    if (wlen != len) {
        putchar('f');
    }
    return wlen;
}

/*
*********************************************************************
*                  Audio AEC Output Read
* Description: 读取aec模块的输出数据
* Arguments  : buf  读取数据存放地址
*			   len	读取数据长度
* Return	 : 数据读取长度
* Note(s)    : None.
*********************************************************************
*/
int audio_aec_output_read(s16 *buf, u16 len)
{
    //printf("rlen:%d-%d\n",len,aec_hdl.output_cbuf.data_len);
    local_irq_disable();
    if (!aec_hdl || !aec_hdl->start) {
        printf("audio_aec close now");
        local_irq_enable();
        return -EINVAL;
    }
    u16 rlen = cbuf_read(&aec_hdl->output_cbuf, buf, len);
    if (rlen == 0) {
        //putchar('N');
    }
    local_irq_enable();
    return rlen;
}

static const char *CVP_ModuleName[] = {
    "AEC", "NLP", "ANS", "ENC", "AGC"
};
static const char *CVP_ModuleStatus[] = {
    "Disable", "Enable"
};
static void dumpModulaStatus(int EnableBit)
{
    printf("EnableBitDebug = 0x%x\n", EnableBit);
    for (int i = 0; i < 5; i++) {
        int k = 1 << i;
        printf("%s : %s\n", CVP_ModuleName[i], CVP_ModuleStatus[((EnableBit & k) != 0)]);
    }
}

/*
*********************************************************************
*                  Audio AEC Parameters
* Description: AEC模块配置参数
* Arguments  : p	参数指针
* Return	 : None.
* Note(s)    : 读取配置文件成功，则使用配置文件的参数配置，否则使用默
*			   认参数配置
*********************************************************************
*/
static void audio_aec_param_init(struct aec_s_attr *p)
{
    int ret = 0;
    AEC_CONFIG cfg;
#if TCFG_AEC_TOOL_ONLINE_ENABLE
    ret = aec_cfg_online_update_fill(&cfg, sizeof(AEC_CONFIG));
#endif/*TCFG_AEC_TOOL_ONLINE_ENABLE*/
    if (ret == 0) {
#if (TCFG_AUDIO_CVP_NS_MODE == CVP_ANS_MODE)
        ret = syscfg_read(CFG_AEC_ID, &cfg, sizeof(AEC_CONFIG));
#else
        ret = syscfg_read(CFG_SMS_DNS_ID, &cfg, sizeof(AEC_CONFIG));
#endif/*TCFG_AUDIO_CVP_NS_MODE*/
    }
    log_info("CVP_NS_MODE = %d\n", TCFG_AUDIO_CVP_NS_MODE);
    if (ret == sizeof(AEC_CONFIG)) {
        log_info("audio_aec read config ok\n");
        p->AGC_NDT_fade_in_step = cfg.ndt_fade_in;
        p->AGC_NDT_fade_out_step = cfg.ndt_fade_out;
        p->AGC_DT_fade_in_step = cfg.dt_fade_in;
        p->AGC_DT_fade_out_step = cfg.dt_fade_out;
        p->AGC_NDT_max_gain = cfg.ndt_max_gain;
        p->AGC_NDT_min_gain = cfg.ndt_min_gain;
        p->AGC_NDT_speech_thr = cfg.ndt_speech_thr;
        p->AGC_DT_max_gain = cfg.dt_max_gain;
        p->AGC_DT_min_gain = cfg.dt_min_gain;
        p->AGC_DT_speech_thr = cfg.dt_speech_thr;
        p->AGC_echo_present_thr = cfg.echo_present_thr;
        p->AEC_DT_AggressiveFactor = cfg.aec_dt_aggress;
        p->AEC_RefEngThr = cfg.aec_refengthr;
        p->ES_AggressFactor = cfg.es_aggress_factor;
        p->ES_MinSuppress = cfg.es_min_suppress;
        p->ES_Unconverge_OverDrive = cfg.es_min_suppress;
        p->ANS_AggressFactor = cfg.ans_aggress;
        p->ANS_MinSuppress = cfg.ans_suppress;
        p->DNS_OverDrive = cfg.ans_aggress;
        p->DNS_GainFloor = cfg.ans_suppress;
        p->DNS_highGain = 2.5f;
        p->DNS_rbRate = 0.3f;
        /* dumpModulaStatus(cfg.aec_mode); */
        p->EnableBit = (cfg.aec_mode & AEC_MODE_ADVANCE);
        if (p->EnableBit == 0) {
            p->toggle = 0;
            printf("cvp toggle off\n");
        } else {
            p->toggle = 1;
        }
        p->agc_en = (cfg.aec_mode & AGC_EN) ? 1 : 0;
        p->ul_eq_en = cfg.ul_eq_en;
        printf("aec_mode:%x,agc_en:%d,ul_eq_en:%d\n", cfg.aec_mode, p->agc_en, p->ul_eq_en);
        //aec_param_dump(p);
    } else {
        log_error("read audio_aec param err:%x", ret);
        p->toggle = 1;
        p->EnableBit = AEC_MODE_REDUCE;
        p->wideband = 0;
        p->ul_eq_en = 1;

        /*AGC*/
        p->agc_en = 1;
        p->AGC_NDT_fade_in_step = 1.3f;
        p->AGC_NDT_fade_out_step = 0.9f;
        p->AGC_DT_fade_in_step = 1.3f;
        p->AGC_DT_fade_out_step = 0.9f;
        p->AGC_NDT_max_gain = 12.f;
        p->AGC_NDT_min_gain = 0.f;
        p->AGC_NDT_speech_thr = -50.f;
        p->AGC_DT_max_gain = 12.f;
        p->AGC_DT_min_gain = 0.f;
        p->AGC_DT_speech_thr = -40.f;
        p->AGC_echo_present_thr = -70.f;

        /*AEC*/
        p->AEC_DT_AggressiveFactor = 1.f;	/*范围：1~5，越大追踪越好，但会不稳定,如破音*/
        p->AEC_RefEngThr = -70.f;

        /*ES*/
        p->ES_AggressFactor = -3.0f;
        p->ES_MinSuppress = 4.f;
        p->ES_Unconverge_OverDrive = p->ES_MinSuppress;

        /*ANS*/
        p->ANS_AggressFactor = 1.25f;	/*范围：1~2,动态调整,越大越强(1.25f)*/
        p->ANS_MinSuppress = 0.04f;	/*范围：0~1,静态定死最小调整,越小越强(0.09f)*/

        /*DNS*/
        p->DNS_GainFloor = 0.1f; /*增益最小值控制，范围：0~1.0，越小降噪越强，默认0.1，建议值:0~0.2*/
        p->DNS_OverDrive = 1.0f;   /*控制降噪强度，范围：0~3.0，越大降噪越强，正常降噪时为1.0，建议调节范围:0.3~3*/
        p->DNS_highGain = 2.5f; /*EQ强度, 范围：1.0f~3.5f,越大越强*/
        p->DNS_rbRate = 0.3f;   /*混响强度，范围：0~0.9f,越大越强*/
    }
    p->ANS_mode = 1;
    p->ANS_NoiseLevel = 2.2e3f;
    p->wn_gain = 331;
    p->SimplexTail = AEC_SIMPLEX_TAIL;
    p->SimplexThr = AEC_SIMPLEX_THR;
    p->dly_est = 0;
    p->dst_delay = 50;
    p->AGC_echo_look_ahead = 0;
    p->AGC_echo_hold = 0;
    p->packet_dump = 50;/*0~255(u8)*/
}

/*
*********************************************************************
*                  Audio AEC Open
* Description: 初始化AEC模块
* Arguments  : sr 			采样率(8000/16000)
*			   enablebit	使能模块(AEC/NLP/AGC/ANS...)
*			   out_hdl		自定义回调函数，NULL则用默认的回调
* Return	 : 0 成功 其他 失败
* Note(s)    : 该接口是对audio_aec_init的扩展，支持自定义使能模块以及
*			   数据输出回调函数
*********************************************************************
*/
int audio_aec_open(u16 sample_rate, s16 enablebit, int (*out_hdl)(s16 *data, u16 len))
{
    struct aec_s_attr *aec_param;
    printf("audio_aec_open\n");
    mem_stats();
#if AEC_USER_MALLOC_ENABLE
    aec_hdl = zalloc(sizeof(struct audio_aec_hdl));
    if (aec_hdl == NULL) {
        log_error("aec_hdl malloc failed");
        return -ENOMEM;
    }
#endif/*AEC_USER_MALLOC_ENABLE*/

    extern u32 aec_addr[], aec_begin[], aec_size[];
    memcpy(aec_addr, aec_begin, (u32)aec_size) ;

#if CVP_LOUDNESS_TRACE_ENABLE
    loudness_meter_init(&mic_loudness, sample_rate, 50, 0);
#endif/*CVP_LOUDNESS_TRACE_ENABLE*/

    cbuf_init(&aec_hdl->output_cbuf, aec_hdl->output_buf, sizeof(aec_hdl->output_buf));
    aec_hdl->dump_packet = AEC_OUT_DUMP_PACKET;
    aec_hdl->inbuf_clear_cnt = AEC_IN_DUMP_PACKET;
    aec_hdl->output_fade_in = 1;
    aec_hdl->output_fade_in_gain = 0;
    aec_param = &aec_hdl->attr;
    aec_param->aec_probe = audio_aec_probe;
    aec_param->aec_post = audio_aec_post;
#if TCFG_AEC_TOOL_ONLINE_ENABLE
    aec_param->aec_update = audio_aec_update;
#endif/*TCFG_AEC_TOOL_ONLINE_ENABLE*/
    aec_param->output_handle = audio_aec_output;
    aec_param->far_noise_gate = 10;

    audio_aec_param_init(aec_param);
#if TCFG_AUDIO_DMS_DUT_ENABLE
    dms_spp_init();
#endif /*TCFG_AUDIO_DMS_DUT_ENABLE*/
    if (enablebit >= 0) {
        aec_param->EnableBit = enablebit;
    }
    if (out_hdl) {
        aec_param->output_handle = out_hdl;
    }

#if TCFG_AEC_SIMPLEX
    aec_param->wn_en = 1;
    aec_param.EnableBit = AEC_MODE_SIMPLEX;
    if (sr == 8000) {
        aec_param.SimplexTail = aec_param.SimplexTail / 2;
    }
#else
    aec_param->wn_en = 0;
#endif/*TCFG_AEC_SIMPLEX*/

    int aec_clk = BT_CALL_16k_ADVANCE_HZ;
    if (sample_rate == 16000) {
        aec_param->wideband = 1;
        aec_param->hw_delay_offset = 60;
        if (aec_param->EnableBit == AEC_MODE_ADVANCE) {
            aec_clk = BT_CALL_16k_ADVANCE_HZ;
        } else {
            aec_clk = BT_CALL_16k_HZ;
        }
    } else {
        aec_param->wideband = 0;
        aec_param->hw_delay_offset = 55;
        if (aec_param->EnableBit == AEC_MODE_ADVANCE) {
            aec_clk = BT_CALL_ADVANCE_HZ;
        } else {
            aec_clk = BT_CALL_HZ;
        }
    }

    if (CONST_ANS_VERSION == ANS_V200) {
        if (aec_param->EnableBit == AEC_MODE_ADVANCE) {
            aec_clk = CONFIG_BT_CALL_DNS_ADVANCE_HZ;
        } else {
            aec_clk = CONFIG_BT_CALL_DNS_HZ;
        }
    }
    printf("aec_clk:%d,EnableBit:%x\n", aec_clk, aec_param->EnableBit);
    clk_set_sys_lock(aec_clk, 1);

#if AEC_UL_EQ_EN
    if (aec_param->ul_eq_en) {
        memset(&aec_hdl->ul_eq, 0, sizeof(struct audio_eq));
        memset(&aec_hdl->ul_eq_ch, 0, sizeof(struct hw_eq_ch));
        aec_hdl->ul_eq.eq_ch = &aec_hdl->ul_eq_ch;
        struct audio_eq_param ul_eq_param;
        ul_eq_param.channels = 1;
        ul_eq_param.online_en = 1;
        ul_eq_param.mode_en = 0;
        ul_eq_param.remain_en = 0;
        ul_eq_param.max_nsection = EQ_SECTION_MAX;
        ul_eq_param.cb = aec_ul_eq_filter;
        ul_eq_param.eq_name = AUDIO_AEC_EQ_NAME;
        audio_eq_open(&aec_hdl->ul_eq, &ul_eq_param);
        audio_eq_set_samplerate(&aec_hdl->ul_eq, sample_rate);
        audio_eq_set_output_handle(&aec_hdl->ul_eq, ul_eq_output, NULL);
        audio_eq_start(&aec_hdl->ul_eq);
    }
#endif

#if AEC_DCCS_EN
    if (adc_data.mic_capless) {
        memset(&aec_hdl->dccs_eq, 0, sizeof(struct audio_eq));
        memset(&aec_hdl->dccs_eq_ch, 0, sizeof(struct hw_eq_ch));
        aec_hdl->dccs_eq.eq_ch = &aec_hdl->dccs_eq_ch;
        struct audio_eq_param dccs_eq_param;
        dccs_eq_param.channels = 1;
        dccs_eq_param.online_en = 0;
        dccs_eq_param.mode_en = 0;
        dccs_eq_param.remain_en = 0;
        dccs_eq_param.max_nsection = EQ_SECTION_MAX;
        dccs_eq_param.cb = aec_dccs_eq_filter;
        audio_eq_open(&aec_hdl->dccs_eq, &dccs_eq_param);
        audio_eq_set_samplerate(&aec_hdl->dccs_eq, sample_rate);
        audio_eq_set_output_handle(&aec_hdl->dccs_eq, dccs_eq_output, NULL);
        audio_eq_start(&aec_hdl->dccs_eq);
    }
#endif

#if AEC_PITCHSHIFTER_CONFIG
    PITCH_SHIFT_PARM sparm = {0};
    sparm.sr = sample_rate;
    sparm.shiftv =  100;
    sparm.effect_v =  EFFECT_PITCH_SHIFT;
    sparm.formant_shift = 100;
    aec_hdl->pitch = open_pitch(&sparm);
    pause_pitch(aec_hdl->pitch, 1);//默认关闭
    aec_hdl->pitch_mode_L = 0;
    aec_hdl->pitch_mode_R = 0;

#endif/* AEC_PITCHSHIFTER_CONFIG */
    //aec_param_dump(aec_param);
    aec_hdl->EnableBit = aec_param->EnableBit;
#if TCFG_KWS_VOICE_RECOGNITION_ENABLE
    extern u8 kws_aec_get_state(void);
    if (kws_aec_get_state()) {
        aec_param->EnableBit = AEC_EN;
        aec_param->agc_en = 0;
        printf("kws open,aec_enablebit=%x", aec_param->EnableBit);
        //临时关闭aec, 对比测试
        //aec_param->toggle = 0;
    }
#endif/*TCFG_KWS_VOICE_RECOGNITION_ENABLE*/


#if AEC_TOGGLE
#ifdef TEST_ZALLOC_MUX_RX_BULK
    test_mux_rx_bulk0 = zalloc_mux_rx_bulk(2 * 1024);
    test_mux_rx_bulk1 = zalloc_mux_rx_bulk(1 * 1024);
    test_mux_rx_bulk2 = zalloc_mux_rx_bulk(2 * 1024 + 512);
    r_printf("test_mux_rx_bulk=%x,%x,%x\n", test_mux_rx_bulk0, test_mux_rx_bulk1, test_mux_rx_bulk2);

    memset(test_mux_rx_bulk0, 0xaa, 2 * 1024);
    memset(test_mux_rx_bulk1, 0x55, 1 * 1024);
    memset(test_mux_rx_bulk2, 0xa5, 2 * 1024);
#endif
    int ret = aec_open(aec_param);
    ASSERT(ret != -EPERM, "Chip not support aec mode!!");
#endif
    aec_hdl->start = 1;
    mem_stats();
    printf("audio_aec_open succ\n");
    return 0;
}

/*
*********************************************************************
*                  Audio AEC Init
* Description: 初始化AEC模块
* Arguments  : sample_rate 采样率(8000/16000)
* Return	 : 0 成功 其他 失败
* Note(s)    : None.
*********************************************************************
*/
int audio_aec_init(u16 sample_rate)
{
    return audio_aec_open(sample_rate, -1, NULL);
}

/*
*********************************************************************
*                  Audio AEC Reboot
* Description: AEC模块复位接口
* Arguments  : reduce 复位/恢复标志
* Return	 : None.
* Note(s)    : None.
*********************************************************************
*/
void audio_aec_reboot(u8 reduce)
{
    if (aec_hdl) {
        printf("audio_aec_reboot:%x,%x,start:%d", aec_hdl->EnableBit, aec_hdl->attr.EnableBit, aec_hdl->start);
        if (aec_hdl->start) {
            if (reduce) {
                aec_hdl->attr.EnableBit = AEC_EN;
                aec_hdl->attr.agc_en = 0;
                aec_reboot(aec_hdl->attr.EnableBit);
            } else {
                if (aec_hdl->EnableBit != aec_hdl->attr.EnableBit) {
                    aec_hdl->attr.agc_en = 1;
                    aec_reboot(aec_hdl->EnableBit);
                }
            }
        }
    } else {
        printf("audio_aec close now\n");
    }
}

/*
*********************************************************************
*                  Audio AEC Close
* Description: 关闭AEC模块
* Arguments  : None.
* Return	 : None.
* Note(s)    : None.
*********************************************************************
*/
void audio_aec_close(void)
{
    printf("audio_aec_close:%x", (u32)aec_hdl);
    if (aec_hdl) {
        aec_hdl->start = 0;
#if AEC_TOGGLE
#ifdef TEST_ZALLOC_MUX_RX_BULK
        if (test_mux_rx_bulk0) {
            free_mux_rx_bulk(test_mux_rx_bulk0);
            test_mux_rx_bulk0 = NULL;
        }
        if (test_mux_rx_bulk1) {
            free_mux_rx_bulk(test_mux_rx_bulk1);
            test_mux_rx_bulk1 = NULL;
        }
        if (test_mux_rx_bulk2) {
            free_mux_rx_bulk(test_mux_rx_bulk2);
            test_mux_rx_bulk2 = NULL;
        }
#endif
        aec_close();
#endif
#if AEC_DCCS_EN
        if (aec_hdl->dccs_eq.start) {
            audio_eq_close(&aec_hdl->dccs_eq);
        }
#endif/*AEC_DCCS_EN*/

#if AEC_UL_EQ_EN
        if (aec_hdl->ul_eq.start) {
            audio_eq_close(&aec_hdl->ul_eq);
        }
#endif/*AEC_UL_EQ_EN*/

#if AEC_PITCHSHIFTER_CONFIG
        if (aec_hdl && aec_hdl->pitch) {
            close_pitch(aec_hdl->pitch);
            aec_hdl->pitch = NULL;
        }
#endif/* AEC_PITCHSHIFTER_CONFIG */
        local_irq_disable();
#if AEC_USER_MALLOC_ENABLE
        free(aec_hdl);
#endif/*AEC_USER_MALLOC_ENABLE*/
        aec_hdl = NULL;
        local_irq_enable();
    }
#if TCFG_AUDIO_DMS_DUT_ENABLE
    dms_spp_unit();
#endif /*TCFG_AUDIO_DMS_DUT_ENABLE*/
}

/*
*********************************************************************
*                  Audio AEC Status
* Description: AEC模块当前状态
* Arguments  : None.
* Return	 : 0 关闭 其他 打开
* Note(s)    : None.
*********************************************************************
*/
u8 audio_aec_status(void)
{
    if (aec_hdl) {
        return aec_hdl->start;
    }
    return 0;
}

/*
*********************************************************************
*                  Audio AEC Input
* Description: AEC源数据输入
* Arguments  : buf	输入源数据地址
*			   len	输入源数据长度
* Return	 : None.
* Note(s)    : 输入一帧数据，唤醒一次运行任务处理数据，默认帧长256点
*********************************************************************
*/
void audio_aec_inbuf(s16 *buf, u16 len)
{
    if (aec_hdl && aec_hdl->start) {
        if (aec_hdl->input_clear) {
            memset(buf, 0, len);
        }
#if AEC_TOGGLE
        if (aec_hdl->inbuf_clear_cnt) {
            aec_hdl->inbuf_clear_cnt--;
            memset(buf, 0, len);
        }
        int ret = aec_in_data(buf, len);
        if (ret == -1) {
        } else if (ret == -2) {
            log_error("aec inbuf full\n");
        }
#else	/*不经算法，直通到输出*/
        audio_aec_output(buf, len);
#endif/*AEC_TOGGLE*/
    }
}

/*
*********************************************************************
*                  Audio AEC Input Reference
* Description: AEC源参考数据输入
* Arguments  : buf	输入源数据地址
*			   len	输入源数据长度
* Return	 : None.
* Note(s)    : 双mic ENC的参考mic数据输入,单mic的无须调用该接口
*********************************************************************
*/
void audio_aec_inbuf_ref(s16 *buf, u16 len)
{
    if (aec_hdl && aec_hdl->start) {
        aec_in_data_ref(buf, len);
    }
}

/*
*********************************************************************
*                  Audio AEC Reference
* Description: AEC模块参考数据输入
* Arguments  : buf	输入参考数据地址
*			   len	输入参考数据长度
* Return	 : None.
* Note(s)    : 声卡设备是DAC，默认不用外部提供参考数据
*********************************************************************
*/
void audio_aec_refbuf(s16 *buf, u16 len)
{
    if (aec_hdl && aec_hdl->start) {
#if AEC_TOGGLE
        aec_ref_data(buf, len);
#endif/*AEC_TOGGLE*/
    }
}

/**
 * 以下为用户层扩展接口
 */
//pbg profile use it,don't delete
void aec_input_clear_enable(u8 enable)
{
    if (aec_hdl) {
        aec_hdl->input_clear = enable;
        log_info("aec_input_clear_enable= %d\n", enable);
    }
}

#if AEC_PITCHSHIFTER_CONFIG

/*----------------------------------------------------------------------------*/
/**@brief    变声参数直接更新
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static void set_pitch_para(u32 shiftv, u32 sr, u8 effect, u32 formant_shift)
{
    if (aec_hdl && aec_hdl->pitch) {
        PITCH_SHIFT_PARM parm = {0};

        parm.sr = sr;
        parm.shiftv = shiftv;
        parm.effect_v = effect;
        parm.formant_shift = formant_shift;

        //printf("\n\n\nshiftv[%d],sr[%d],effect[%d],formant_shift[%d] \n\n", parm.shiftv, parm.sr, parm.effect_v, parm.formant_shift);
        update_picth_parm(aec_hdl->pitch, &parm);
    }
}

/*----------------------------------------------------------------------------*/
/**@brief    变声效果设置
   @param  sound:原声SOUND_ORIGINAL,男声SOUND_UNCLE, 女声SOUND_GODDESS
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void audio_aec_pitch_change(u32 sound)
{
    if (aec_hdl && aec_hdl->pitch) {
        switch (sound) {
        case SOUND_ORIGINAL:
            pause_pitch(aec_hdl->pitch, 1);
            log_info("pitch origin\n");
            break;
        case SOUND_UNCLE:
            set_pitch_para(136, aec_hdl->pitch->parm.sr, EFFECT_PITCH_SHIFT, 0);
            pause_pitch(aec_hdl->pitch, 0);
            log_info("pitch uncle\n");
            break;
        case SOUND_GODDESS:
            if (VC_KINDO_TVM) {
                set_pitch_para(56, aec_hdl->pitch->parm.sr, EFFECT_VOICECHANGE_KIN0, 150);
            } else {
                set_pitch_para(46, aec_hdl->pitch->parm.sr, EFFECT_VOICECHANGE_KIN2, 80);
            }
            /* set_pitch_para(66, aec_hdl->pitch->parm.sr, EFFECT_VOICECHANGE_KIN0, 150); */
            pause_pitch(aec_hdl->pitch, 0);
            log_info("pitch goddess\n");
            break;
        default:
            break;
        }
    }
}

struct _esco_eff {
    u16 eff_L;
    u16 eff_R;
};
struct _esco_eff esco_eff;
#define TWS_FUNC_ID_ESCO_EFF \
	((int)(('E' + 'S' + 'C' + 'O') << (2 * 8)) | \
	 (int)(('E' + 'F' + 'F') << (1 * 8)) | \
	 (int)(('S' + 'Y' + 'N' + 'C') << (0 * 8)))


/*----------------------------------------------------------------------------*/
/**@brief   变声控制
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void audio_aec_pitch_change_ctrl()
{
    if (!aec_hdl) {
        return;
    }
#if TCFG_USER_TWS_ENABLE
    int state = tws_api_get_tws_state();
    if (state & TWS_STA_SIBLING_CONNECTED) {// tws链接时，左耳触发男声， 右耳触发女声
        enum audio_channel channel = tws_api_get_local_channel() == 'L' ? AUDIO_CH_L : AUDIO_CH_R;
        if (channel == AUDIO_CH_L) {
            log_info("aec_hdl->pitch_mode_L %d\n", aec_hdl->pitch_mode_L);
            if (aec_hdl->pitch_mode_L != SOUND_UNCLE) {
                aec_hdl->pitch_mode_L = SOUND_UNCLE;
                aec_hdl->pitch_mode_R = SOUND_UNCLE;
            } else {
                aec_hdl->pitch_mode_L = SOUND_ORIGINAL;
                aec_hdl->pitch_mode_R = SOUND_ORIGINAL;
            }
        } else if (channel == AUDIO_CH_R) {
            log_info("aec_hdl->pitch_mode_R %d\n", aec_hdl->pitch_mode_R);
            if (aec_hdl->pitch_mode_R != SOUND_GODDESS) {
                aec_hdl->pitch_mode_R = SOUND_GODDESS;
                aec_hdl->pitch_mode_L = SOUND_GODDESS;
            } else {
                aec_hdl->pitch_mode_R = SOUND_ORIGINAL;
                aec_hdl->pitch_mode_L = SOUND_ORIGINAL;
            }
        }
        esco_eff.eff_L = aec_hdl->pitch_mode_L;
        esco_eff.eff_R = aec_hdl->pitch_mode_R;
        tws_api_send_data_to_sibling((u8 *)&esco_eff, sizeof(struct _esco_eff), TWS_FUNC_ID_ESCO_EFF);//发送左右耳的变声模式
    } else
#endif/* TCFG_USER_TWS_ENABLE */
    {
        //tws未连接时，变声直接切换，男声->女声->原声
        if (aec_hdl->pitch_mode_L != SOUND_UNCLE) {
            aec_hdl->pitch_mode_L = SOUND_UNCLE;
        } else if (aec_hdl->pitch_mode_L == SOUND_UNCLE) {
            aec_hdl->pitch_mode_L = SOUND_GODDESS;
        } else {
            aec_hdl->pitch_mode_L = SOUND_ORIGINAL;
        }
        audio_aec_pitch_change(aec_hdl->pitch_mode_L);
    }
}


#if TCFG_USER_TWS_ENABLE
static void tws_esco_pitch_align(void *data, u16 len, bool rx)
{
    memcpy(&esco_eff, data, sizeof(struct _esco_eff));
    int state = tws_api_get_tws_state();
    enum audio_channel channel;
    if (state & TWS_STA_SIBLING_CONNECTED) {
        channel = tws_api_get_local_channel() == 'L' ? AUDIO_CH_L : AUDIO_CH_R;
        if (channel == AUDIO_CH_L) {//左耳设置变声
            log_info("channel %d============= eff_L %d\n", channel, esco_eff.eff_L);
            audio_aec_pitch_change(esco_eff.eff_L);
            if (aec_hdl) {
                aec_hdl->pitch_mode_L =  esco_eff.eff_L;
            }
        } else {//右耳设置变声
            log_info("channel %d============= eff_R %d\n", channel, esco_eff.eff_R);
            audio_aec_pitch_change(esco_eff.eff_R);
            if (aec_hdl) {
                aec_hdl->pitch_mode_R =  esco_eff.eff_R;
            }
        }
    }
}

REGISTER_TWS_FUNC_STUB(esco_align_eff) = {
    .func_id = TWS_FUNC_ID_ESCO_EFF,
    .func    = tws_esco_pitch_align,
};

#endif /* TCFG_USER_TWS_ENABLE */

#endif /* AEC_PITCHSHIFTER_CONFIG */


#endif/*TCFG_AUDIO_DUAL_MIC_ENABLE == 0*/
#endif /*TCFG_CVP_DEVELOP_ENABLE*/
