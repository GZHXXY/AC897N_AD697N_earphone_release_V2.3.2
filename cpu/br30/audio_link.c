/*****************************************************************
>file name : lib/media/cpu/br22/audio_link.c
>author :
>create time : Fri 7 Dec 2018 14:59:12 PM CST
*****************************************************************/
#include "includes.h"
#include "asm/includes.h"
#include "media/includes.h"
#include "system/includes.h"
#include "app_config.h"
#include "asm/clock.h"
#include "asm/iis.h"
#include "audio_link.h"

#if TCFG_AUDIO_INPUT_IIS || TCFG_AUDIO_OUTPUT_IIS

#define ALINK_TEST_ENABLE

#define ALINK_DEBUG_INFO
#ifdef ALINK_DEBUG_INFO
#define alink_printf  printf
#else
#define alink_printf(...)
#endif

static u32 *ALNK_BUF_ADR[] = {
    (u32 *)(&(JL_ALNK0->ADR0)),
    (u32 *)(&(JL_ALNK0->ADR1)),
    (u32 *)(&(JL_ALNK0->ADR2)),
    (u32 *)(&(JL_ALNK0->ADR3)),
};

static u32 PFI_ALNK0_DAT[] = {
    PFI_ALNK0_DAT0,
    PFI_ALNK0_DAT1,
    PFI_ALNK0_DAT2,
    PFI_ALNK0_DAT3,
};

static u32 FO_ALNK0_DAT[] = {
    FO_ALNK0_DAT0,
    FO_ALNK0_DAT1,
    FO_ALNK0_DAT2,
    FO_ALNK0_DAT3,
};


enum {
    MCLK_11M2896K = 0,
    MCLK_12M288K
};

enum {
    MCLK_EXTERNAL	= 0u,
    MCLK_SYS_CLK		,
    MCLK_OSC_CLK 		,
    MCLK_PLL_CLK		,
};

enum {
    MCLK_DIV_1		= 0u,
    MCLK_DIV_2			,
    MCLK_DIV_4			,
    MCLK_DIV_8			,
    MCLK_DIV_16			,
};

enum {
    MCLK_LRDIV_EX	= 0u,
    MCLK_LRDIV_64FS		,
    MCLK_LRDIV_128FS	,
    MCLK_LRDIV_192FS	,
    MCLK_LRDIV_256FS	,
    MCLK_LRDIV_384FS	,
    MCLK_LRDIV_512FS	,
    MCLK_LRDIV_768FS	,
};

ALINK_PARM *p_alink_parm;

//==================================================
static void alink_clk_io_in_init(u8 gpio)
{
    gpio_set_direction(gpio, 1);
    gpio_set_pull_down(gpio, 0);
    gpio_set_pull_up(gpio, 1);
    gpio_set_die(gpio, 1);
}


sec(.volatile_ram_code)
static void *alink_addr(u8 ch)
{
    u8 *buf_addr = NULL; //can be used
    u32 buf_index = 0;
    buf_addr = (u8 *)(p_alink_parm->ch_cfg[ch].buf);

    u8 index_table[4] = {12, 13, 14, 15};
    u8 bit_index = index_table[ch];
    buf_index = (JL_ALNK0->CON0 & BIT(bit_index)) ? 0 : 1;
    buf_addr = buf_addr + ((p_alink_parm->dma_len / 2) * buf_index);

    return buf_addr;
}

sec(.volatile_ram_code)
___interrupt
static void alink_isr(void)
{
    u16 reg;
    s16 *buf_addr = NULL ;
    u8 ch = 0;

    reg = JL_ALNK0->CON2;

    //channel 0
    if (reg & BIT(4)) {
        ch = 0;
        ALINK_CLR_CH0_PND();
        buf_addr = alink_addr(ALINK_CH0);
        goto __isr_cb;
    }
    //channel 1
    if (reg & BIT(5)) {
        ch = 1;
        ALINK_CLR_CH1_PND();
        buf_addr = alink_addr(ALINK_CH1);
        goto __isr_cb;
    }
    //channel 2
    if (reg & BIT(6)) {
        ch = 2;
        ALINK_CLR_CH2_PND();
        buf_addr = alink_addr(ALINK_CH2);
        goto __isr_cb;
    }
    //channel 3
    if (reg & BIT(7)) {
        ch = 3;
        ALINK_CLR_CH3_PND();
        buf_addr = alink_addr(ALINK_CH3);
        goto __isr_cb;
    }

__isr_cb:
    if (p_alink_parm->ch_cfg[ch].isr_cb) {
        p_alink_parm->ch_cfg[ch].isr_cb(buf_addr, p_alink_parm->dma_len / 2);
    }
}

static void alink_sr(u32 rate)
{
    alink_printf("ALINK_SR = %d\n", rate);
    u8 pll_target_frequency = clock_get_pll_target_frequency();
    alink_printf("pll_target_frequency = %d\n", pll_target_frequency);
    switch (rate) {
    case ALINK_SR_48000:
        /*12.288Mhz 256fs*/
        audio_link_clock_sel(ALINK_CLOCK_12M288K);
        ALINK_MDIV(MCLK_DIV_1);
        ALINK_LRDIV(MCLK_LRDIV_256FS);
        break ;

    case ALINK_SR_44100:
        /*11.289Mhz 256fs*/
        audio_link_clock_sel(ALINK_CLOCK_11M2896K);
        ALINK_MDIV(MCLK_DIV_1);
        if (pll_target_frequency == 192) {
            ALINK_LRDIV(MCLK_LRDIV_256FS);
        } else if (pll_target_frequency == 240) {
            ALINK_LRDIV(MCLK_LRDIV_128FS);
        }
        break ;

    case ALINK_SR_32000:
        /*12.288Mhz 384fs*/
        audio_link_clock_sel(ALINK_CLOCK_12M288K);
        ALINK_MDIV(MCLK_DIV_1);
        ALINK_LRDIV(MCLK_LRDIV_384FS);
        break ;

    case ALINK_SR_24000:
        /*12.288Mhz 512fs*/
        audio_link_clock_sel(ALINK_CLOCK_12M288K);
        ALINK_MDIV(MCLK_DIV_1);
        ALINK_LRDIV(MCLK_LRDIV_512FS);
        break ;

    case ALINK_SR_22050:
        /*11.289Mhz 512fs*/
        audio_link_clock_sel(ALINK_CLOCK_11M2896K);
        ALINK_MDIV(MCLK_DIV_1);
        if (pll_target_frequency == 192) {
            ALINK_LRDIV(MCLK_LRDIV_512FS);
        } else if (pll_target_frequency == 240) {
            ALINK_LRDIV(MCLK_LRDIV_256FS);
        }
        break ;

    case ALINK_SR_16000:
        /*12.288/2Mhz 384fs*/
        audio_link_clock_sel(ALINK_CLOCK_12M288K);
        ALINK_MDIV(MCLK_DIV_1);
        ALINK_LRDIV(MCLK_LRDIV_768FS);
        break ;

    case ALINK_SR_12000:
        /*12.288/2Mhz 512fs*/
        audio_link_clock_sel(ALINK_CLOCK_12M288K);
        ALINK_MDIV(MCLK_DIV_2);
        ALINK_LRDIV(MCLK_LRDIV_512FS);
        break ;

    case ALINK_SR_11025:
        /*11.289/2Mhz 512fs*/
        audio_link_clock_sel(ALINK_CLOCK_11M2896K);
        ALINK_MDIV(MCLK_DIV_2);
        if (pll_target_frequency == 192) {
            ALINK_LRDIV(MCLK_LRDIV_512FS);
        } else if (pll_target_frequency == 240) {
            ALINK_LRDIV(MCLK_LRDIV_256FS);
        }
        break ;

    case ALINK_SR_8000:
        /*12.288/4Mhz 384fs*/
        audio_link_clock_sel(ALINK_CLOCK_12M288K);
        ALINK_MDIV(MCLK_DIV_2);
        ALINK_LRDIV(MCLK_LRDIV_768FS);
        break ;

    default:
        //44100
        /*11.289Mhz 256fs*/
        audio_link_clock_sel(ALINK_CLOCK_11M2896K);
        ALINK_MDIV(MCLK_DIV_1);
        ALINK_LRDIV(MCLK_LRDIV_128FS);
        break;
    }
    if (p_alink_parm->role == ALINK_ROLE_SLAVE) {
        ALINK_LRDIV(MCLK_LRDIV_EX);
    }
}


void alink_channel_init(u8 ch_idx, u8 dir, void (*handle)(s16 *buf, u32 len))
{
    /* u8 i = 0; */
    p_alink_parm->ch_cfg[ch_idx].dir = dir;
    p_alink_parm->ch_cfg[ch_idx].isr_cb = handle;
    p_alink_parm->ch_cfg[ch_idx].buf = malloc(p_alink_parm->dma_len);
    u32 *buf_addr;
    //===================================//
    //           ALNK工作模式            //
    //===================================//
    if (p_alink_parm->mode > ALINK_MD_IIS_RALIGN) {
        ALINK_CHx_MODE_SEL((p_alink_parm->mode - ALINK_MD_IIS_RALIGN), ch_idx);
    } else {
        ALINK_CHx_MODE_SEL(p_alink_parm->mode, ch_idx);
    }
    //===================================//
    //           ALNK CH DMA BUF         //
    //===================================//
    buf_addr = ALNK_BUF_ADR[ch_idx];
    *buf_addr = (u32)(p_alink_parm->ch_cfg[ch_idx].buf);
    //===================================//
    //          ALNK CH DAT IO INIT      //
    //===================================//
    if (p_alink_parm->ch_cfg[ch_idx].dir == ALINK_DIR_RX) {
        gpio_set_direction(p_alink_parm->ch_cfg[ch_idx].data_io, 1);
        gpio_set_pull_down(p_alink_parm->ch_cfg[ch_idx].data_io, 0);
        gpio_set_pull_up(p_alink_parm->ch_cfg[ch_idx].data_io, 1);
        gpio_set_die(p_alink_parm->ch_cfg[ch_idx].data_io, 1);
        gpio_set_fun_input_port(p_alink_parm->ch_cfg[ch_idx].data_io, PFI_ALNK0_DAT[ch_idx]);
        ALINK_CHx_DIR_RX_MODE(ch_idx);
    } else {
        gpio_direction_output(p_alink_parm->ch_cfg[ch_idx].data_io, 1);
        gpio_set_fun_output_port(p_alink_parm->ch_cfg[ch_idx].data_io, FO_ALNK0_DAT[ch_idx], 1, 1);
        ALINK_CHx_DIR_TX_MODE(ch_idx);
    }
    r_printf("alink_ch %d init\n", ch_idx);
}

static void alink_info_dump()
{
    alink_printf("JL_ALNK0->CON0 = 0x%x", JL_ALNK0->CON0);
    alink_printf("JL_ALNK0->CON1 = 0x%x", JL_ALNK0->CON1);
    alink_printf("JL_ALNK0->CON2 = 0x%x", JL_ALNK0->CON2);
    alink_printf("JL_ALNK0->CON3 = 0x%x", JL_ALNK0->CON3);
    alink_printf("JL_ALNK0->LEN = 0x%x", JL_ALNK0->LEN);
    alink_printf("JL_ALNK0->ADR0 = 0x%x", JL_ALNK0->ADR0);
    alink_printf("JL_ALNK0->ADR1 = 0x%x", JL_ALNK0->ADR1);
    alink_printf("JL_ALNK0->ADR2 = 0x%x", JL_ALNK0->ADR2);
    alink_printf("JL_ALNK0->ADR3 = 0x%x", JL_ALNK0->ADR3);
    alink_printf("JL_PLL->PLL_CON2 = 0x%x", JL_PLL->PLL_CON2);
}

int alink_init(ALINK_PARM *parm)
{
    if (parm == NULL) {
        return -1;
    }

    ALNK_CON_RESET();

    p_alink_parm = parm;

    request_irq(IRQ_ALINK0_IDX, 3, alink_isr, 0);
    ALINK_MSRC(MCLK_PLL_CLK);	/*MCLK source*/

    //===================================//
    //        输出时钟配置               //
    //===================================//
    if (parm->role == ALINK_ROLE_MASTER) {
        //主机输出时钟
        if (parm->mclk_io != ALINK_CLK_OUPUT_DISABLE) {
            gpio_direction_output(parm->mclk_io, 1);
            gpio_set_fun_output_port(parm->mclk_io, FO_ALNK0_MCLK, 1, 1);
            ALINK_MOE(1);				/*MCLK output to IO*/
        }
        if ((parm->sclk_io != ALINK_CLK_OUPUT_DISABLE) && (parm->lrclk_io != ALINK_CLK_OUPUT_DISABLE)) {
            gpio_direction_output(parm->lrclk_io, 1);
            gpio_set_fun_output_port(parm->lrclk_io, FO_ALNK0_LRCK, 1, 1);
            gpio_direction_output(parm->sclk_io, 1);
            gpio_set_fun_output_port(parm->sclk_io, FO_ALNK0_SCLK, 1, 1);
            ALINK_SOE(1);				/*SCLK/LRCK output to IO*/
        }
    } else {
        //从机输入时钟
        if (parm->mclk_io != ALINK_CLK_OUPUT_DISABLE) {
            alink_clk_io_in_init(parm->mclk_io);
            gpio_set_fun_input_port(parm->mclk_io, PFI_ALNK0_MCLK);
            ALINK_MOE(0);				/*MCLK input to IO*/
        }
        if ((parm->sclk_io != ALINK_CLK_OUPUT_DISABLE) && (parm->lrclk_io != ALINK_CLK_OUPUT_DISABLE)) {
            alink_clk_io_in_init(parm->lrclk_io);
            gpio_set_fun_input_port(parm->lrclk_io, PFI_ALNK0_LRCK);
            alink_clk_io_in_init(parm->sclk_io);
            gpio_set_fun_input_port(parm->sclk_io, PFI_ALNK0_SCLK);
            ALINK_SOE(0);				/*SCLK/LRCK output to IO*/
        }
    }
    //===================================//
    //        基本模式/扩展模式          //
    //===================================//
    ALINK_DSPE(p_alink_parm->mode >> 2);

    //===================================//
    //         数据位宽16/32bit          //
    //===================================//
    //注意: 配置了24bit, 一定要选ALINK_FRAME_64SCLK, 因为sclk32 x 2才会有24bit;
    if (p_alink_parm->bitwide == ALINK_LEN_24BIT) {
        ASSERT(p_alink_parm->sclk_per_frame == ALINK_FRAME_64SCLK);
        ALINK_24BIT_MODE();
        //一个点需要4bytes, LR = 2, 双buf = 2
        JL_ALNK0->LEN = p_alink_parm->dma_len / 8; //点数
        //JL_ALNK0->LEN = p_alink_parm->dma_len / 16; //点数
    } else {
        ALINK_16BIT_MODE();
        //一个点需要2bytes, LR = 2, 双buf = 2
        JL_ALNK0->LEN = p_alink_parm->dma_len / 8; //点数
    }
    //===================================//
    //             时钟边沿选择          //
    //===================================//
    if (p_alink_parm->clk_mode == ALINK_CLK_FALL_UPDATE_RAISE_SAMPLE) {
        SCLKINV(0);
    } else {
        SCLKINV(1);
    }
    //===================================//
    //            每帧数据sclk个数       //
    //===================================//
    if (p_alink_parm->sclk_per_frame == ALINK_FRAME_64SCLK) {
        F32_EN(0);
    } else {
        F32_EN(1);
    }

    //===================================//
    //            设置数据采样率       	 //
    //===================================//
    alink_sr(p_alink_parm->sample_rate);

    return 0;
}

void alink_channel_close(u8 ch_idx)
{
    gpio_set_pull_up(p_alink_parm->ch_cfg[ch_idx].data_io, 0);
    gpio_set_pull_down(p_alink_parm->ch_cfg[ch_idx].data_io, 0);
    gpio_set_direction(p_alink_parm->ch_cfg[ch_idx].data_io, 1);
    gpio_set_die(p_alink_parm->ch_cfg[ch_idx].data_io, 0);
    ALINK_CHx_CLOSE(0, ch_idx);
    if (p_alink_parm->ch_cfg[ch_idx].buf) {
        free(p_alink_parm->ch_cfg[ch_idx].buf);
        p_alink_parm->ch_cfg[ch_idx].buf = NULL;
    }
    r_printf("alink_ch %d closed\n", ch_idx);
}


int alink_start(void)
{
    if (p_alink_parm) {
        ALINK_EN(1);
        alink_printf("alink_link start\n");
        return 0;
    }
    return -1;
}

void alink_uninit(void)
{
    ALINK_EN(0);
    if ((JL_PLL->PLL_CON2 & 0x2) == 0) {
        PLL_CLK_F2_OE(0);
    }
    if ((JL_PLL->PLL_CON2 & 0xc) == 0) {
        PLL_CLK_F3_OE(0);
    }
    for (int i = 0; i < 4; i++) {
        if (p_alink_parm->ch_cfg[i].buf) {
            alink_channel_close(i);
        }
    }
    p_alink_parm = NULL;
    alink_printf("audio_link_exit OK\n");
}

int alink_sr_set(u16 sr)
{
    if (p_alink_parm) {
        ALINK_EN(0);
        alink_sr(sr);
        ALINK_EN(1);
        return 0;
    } else {
        return -1;
    }
}

extern ALINK_PARM alink_param;
static u8 alink_init_cnt = 0;
void audio_link_init(void)
{
    if (alink_init_cnt == 0) {
        alink_init(&alink_param);
        alink_start();
    }
    alink_init_cnt++;
}
void audio_link_uninit(void)
{
    alink_init_cnt--;
    if (alink_init_cnt == 0) {
        alink_uninit();
    }
}
//===============================================//
//			     for APP use demo                //
//===============================================//

#ifdef ALINK_TEST_ENABLE

u32 data_buf[2][ALNK_BUF_POINTS_NUM * 2] __attribute__((aligned(4)));

void handle_tx(void *buf, u16 len)
{
    putchar('o');
    s16 *data_tx = (s16 *)buf;
    s16 *data = (s16 *)data_buf;
    for (int i = 0; i < len / sizeof(s16) ; i++) {
        data_tx[i] = 0x5a5a;
        /* data_tx[i] = data[i]; */
    }
}

void handle_rx(void *buf, u16 len)
{
    /* put_buf(buf, 32); */
#if 0
    s16 *data_rx = (s16 *)buf;
    s16 *data_b = (s16 *)data_buf;
    for (int i = 0; i < len / sizeof(s16) ; i++) {
        data_b[i] = data_rx[i];
    }
#endif
}

ALINK_PARM	test_alink = {
    .mclk_io = IO_PORTA_02,
    .sclk_io = IO_PORTA_07,
    .lrclk_io = IO_PORTA_00,
    .ch_cfg[0].data_io = IO_PORTA_03,
    .ch_cfg[1].data_io = IO_PORTA_04,
    .mode = ALINK_MD_IIS,
    /* .role = ALINK_ROLE_SLAVE, */
    .role = ALINK_ROLE_MASTER,
    .clk_mode = ALINK_CLK_FALL_UPDATE_RAISE_SAMPLE,
    .bitwide = ALINK_LEN_16BIT,
    .sclk_per_frame = ALINK_FRAME_32SCLK,
    .dma_len = ALNK_BUF_POINTS_NUM * 2 * 2 * 2,
    .sample_rate = TCFG_IIS_SAMPLE_RATE,
    /* .sample_rate = 16000, */
};

void audio_link_test(void)
{
    alink_init(&test_alink);
    alink_channel_init(0, ALINK_DIR_RX, handle_rx);
    alink_channel_init(1, ALINK_DIR_TX, handle_tx);
    alink_start();
    alink_info_dump();
}

/******************* wm8978 mic demo *********************/
/*2021/01/08 qiuhaihui*/
#if 0
extern struct audio_dac_hdl dac_hdl;
//#define IIS_SRC_ENABLE/*同步iis和audio模块的采样率*/
#ifdef IIS_SRC_ENABLE
#include "Resample_api.h"
static RS_STUCT_API *sw_src_api = NULL;
static u8 *sw_src_buf = NULL;
#endif/*IIS_SRC_ENABLE*/
static void wm8978_mic_output(s16 *data, u32 len)
{
#if (defined TCFG_AUDIO_DAC_CONNECT_MODE && (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_LR))
    /*双声道*/
    int wlen =  audio_dac_write(&dac_hdl, data, len);
    if (wlen != len) {
        printf("wlen %d %d\n", wlen, len);
    }

#else /*单声道*/
    /*iis双声道数据变单声道 */
    for (int i = 0; i < len / 2 / 2; i++) {
        data[i] = data[i * 2];
    }
    len >>= 1;

#ifdef IIS_SRC_ENABLE
    if (sw_src_api) {
        len = sw_src_api->run(sw_src_buf, data, len >> 1, data);
        len <<= 1;
    }
#endif/*IIS_SRC_ENABLE*/
    int wlen =  audio_dac_write(&dac_hdl, data, len);
    if (wlen != len) {
        printf("wlen %d %d\n", wlen, len);
    }
#endif /*TCFG_AUDIO_DAC_CONNECT_MODE*/
}

extern u8 WM8978_Init(u8 dacen, u8 adcen);
void WM8978_mic_demo()
{
    /*测试16000采样率输出正常*/

    /***** wm8978模块用法*******/
    /***相关文件：
        apps/common/audio/wm8978/wm8978.h
        apps/common/audio/wm8978/wm8978.c
        apps/common/audio/wm8978/iic.h
        apps/common/audio/wm8978/iic.c
     ***/
    /***硬件连接
     * MLCKA --- PA2
     * SCKA  --- PA7
     * FSA   --- PA0
     * SDB   --- PA3
     * IIC_SDA  -> apps/common/audio/wm8978/iic.h -> IIC_DATA_PORT
     * IIC_SCL  -> apps/common/audio/wm8978/iic.h -> IIC_CLK_PORT
     ****/

    /* wm897模块初始化
     * 主从模式：从机模式
     * 工作模式：飞利浦标准iis
     * 数据位宽：16位
     */
    WM8978_Init(0, 1);

#ifdef IIS_SRC_ENABLE
    sw_src_api = get_rs16_context();
    printf("sw_src_api:0x%x\n", sw_src_api);
    ASSERT(sw_src_api);
    int sw_src_need_buf = sw_src_api->need_buf();
    printf("sw_src_buf:%d\n", sw_src_need_buf);
    sw_src_buf = zalloc(sw_src_need_buf);
    ASSERT(sw_src_buf);
    RS_PARA_STRUCT rs_para_obj;
    rs_para_obj.nch = 1;
    rs_para_obj.new_insample = 625 * 20;
    rs_para_obj.new_outsample = 624 * 20;
    printf("sw src,in = %d,out = %d\n", rs_para_obj.new_insample, rs_para_obj.new_outsample);
    sw_src_api->open(sw_src_buf, &rs_para_obj);
#endif/*IIS_SRC_ENABLE*/

    /*iis初始化*/
    audio_link_init();
    alink_channel_init(0, 1, wm8978_mic_output);
    /*打开dac*/
    audio_dac_set_sample_rate(&dac_hdl, TCFG_IIS_SAMPLE_RATE);
    audio_dac_start(&dac_hdl);
    /*设置数字音量*/
    JL_AUDIO->DAC_VL0 = 0x40004000;
    /*设置模拟音量*/
    u32 avol = 12;
    audio_dac_ch_set_analog_vol(&dac_hdl, DA_LEFT, avol);
    audio_dac_ch_set_analog_vol(&dac_hdl, DA_RIGHT, avol);

}
#endif /*wm8978 mic demo*/
/******************* end wm8978 mic demo ************************/

#endif /*ALINK_TEST_ENABLE*/

#endif /*TCFG_AUDIO_INPUT_IIS || TCFG_AUDIO_OUTPUT_IIS*/

