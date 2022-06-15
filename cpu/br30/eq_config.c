#include "system/includes.h"
#include "media/includes.h"
#include "app_config.h"
#include "app_online_cfg.h"
#include "online_db_deal.h"

#include "application/audio_eq.h"
#include "audio_eq_tab.h"

#include "application/audio_drc.h"
#include "math.h"

#include "cfg_tool.h"
#define LOG_TAG     "[APP-EQ]"
#define LOG_ERROR_ENABLE
#define LOG_INFO_ENABLE
#define LOG_DUMP_ENABLE
#include "debug.h"

#ifdef CONFIG_EQ_APP_SEG_ENABLE
#pragma const_seg(	".eq_app_codec_const")
#pragma code_seg (	".eq_app_codec_code" )
#endif

#if (TCFG_EQ_ENABLE == 1)

extern const u8 audio_eq_sdk_name[16];
extern const u8 audio_eq_ver[4];
const u8 audio_eq_ver_old[4] 			= {0, 7, 1, 0};
const u8 audio_eq_sdk_name[16] 		= "AC897N";
const u8 audio_eq_ver[4] 			= {0, 7, 1, 0};

#define TCFG_EQ_FILE_ENABLE		1
#define EQ_FILE_NAME 			SDFILE_RES_ROOT_PATH"eq_cfg_hw.bin"
#define audio_eq_password       "000"
#define password_en   0
#define EQ_FADE_EN				0	            //EQ慢慢变化

static u8 reply_to_tool = 0;
static u8 reply_sq = 0;

//左右声道drc类型需一致，工具查询时，使用 该数组 来应答给工具，添加限制
static const u16 drc_constriants[] = {
    /* 0x01, 0x02, 0x04, 0x2101, 0x05, 0x2101, */
    /* 0x01, 0x02, 0x06, 0x2101, 0x07, 0x2101, */
};

//一共有多少个模式
#define eq_mode_num  (5)


//每个eq调试模式，拥有的最大段数
#define SONG_EQ_SECTION         EQ_SECTION_MAX
#define CALL_EQ_SECTION         3              //最大EQ_SECTION_MAX段
#define CALL_NARROW_EQ_SECTION  CALL_EQ_SECTION
#define AEC_EQ_SECTION          3              //最大EQ_SECTION_MAX段
#define AEC_NARROW_EQ_SECTION   AEC_EQ_SECTION

#define eq_log  log_i
typedef struct {
    float global_gain;
    int seg_num;
    int enable_section;
} CFG_PARM;

typedef struct {
    CFG_PARM par;
    EQ_CFG_SEG seg[EQ_SECTION_MAX];
} EQ_CFG_PARAMETER;


typedef struct {
#if TCFG_DRC_ENABLE
    struct drc_ch_org drc;
#endif
} DRC_CFG_PARAMETER;


typedef struct {
    /* unsigned short crc; */
    unsigned short id;
    unsigned short len;
} EQ_FILE_HEAD;

typedef struct {
    EQ_FILE_HEAD head;
    EQ_CFG_PARAMETER parm;
}  EQ_CFG_PARM;

typedef struct {
    EQ_FILE_HEAD head;
    DRC_CFG_PARAMETER parm;
}  DRC_CFG_PARM;

typedef struct {
    EQ_CFG_PARM song_eq_parm;
    DRC_CFG_PARM drc_parm;
    u8 cur_mode;
} EQ_FILE_PARM;

typedef struct {
    u32 eq_type : 3;
    u32 mode_updata : 1;
    u32 eq_file_ver_err : 1;
    u32 eq_file_section_err : 1;
    u32 drc_updata[eq_mode_num];
    u32 online_updata[eq_mode_num];
    u32 design_mask[eq_mode_num];
    u32 seg_num[eq_mode_num];
    EQ_FILE_PARM cfg_parm[eq_mode_num];
    u16 cur_sr[eq_mode_num];
    spinlock_t lock;

#ifndef CONFIG_EQ_NO_USE_COEFF_TABLE
    int song_mode_coeff_table[eq_mode_num][EQ_SECTION_MAX * 5];
#endif

#if (TCFG_EQ_ONLINE_ENABLE && TCFG_USER_TWS_ENABLE)
    void *tws_ci;
    int tws_tmr;
    u16 *tws_pack;
    u8 pack_id;

#endif
    u8 mode_id;
    uint8_t password_ok;
} EQ_CFG;


typedef struct {
    u8 fade_stu;
    u16 tmr;
    u16 sr[eq_mode_num];
    EQ_CFG_SEG seg[eq_mode_num][EQ_SECTION_MAX];
} EQ_FADE_CFG;

/*-----------------------------------------------------------*/
/*eq online cmd*/
typedef enum {
    EQ_ONLINE_CMD_SECTION       = 1,
    EQ_ONLINE_CMD_GLOBAL_GAIN,
    EQ_ONLINE_CMD_LIMITER,
    EQ_ONLINE_CMD_INQUIRE,
    EQ_ONLINE_CMD_GETVER,
    EQ_ONLINE_CMD_GET_SOFT_SECTION,            //br22专用
    EQ_ONLINE_CMD_GET_SECTION_NUM = 0x7,       //工具查询 小机需要的eq段数
    EQ_ONLINE_CMD_GLOBAL_GAIN_SUPPORT_FLOAT = 0x8,


    EQ_ONLINE_CMD_PASSWORD = 0x9,
    EQ_ONLINE_CMD_VERIFY_PASSWORD = 0xA,
    EQ_ONLINE_CMD_FILE_SIZE = 0xB,
    EQ_ONLINE_CMD_FILE = 0xC,
    EQ_EQ_ONLINE_CMD_GET_SECTION_NUM = 0xD,     //该命令新加与 0x7功能一样
    EQ_EQ_ONLINE_CMD_CHANGE_MODE = 0xE,         //切模式

    EQ_ONLINE_CMD_MODE_COUNT = 0x100,           //模式个数a  1
    EQ_ONLINE_CMD_MODE_NAME = 0x101,            //模式的名字a eq
    EQ_ONLINE_CMD_MODE_GROUP_COUNT = 0x102,     //模式下组的个数,4个字节 1
    EQ_ONLINE_CMD_MODE_GROUP_RANGE = 0x103,     //模式下组的id内容  0x11
    EQ_ONLINE_CMD_EQ_GROUP_COUNT = 0x104,       //eq组的id个数  1
    EQ_ONLINE_CMD_EQ_GROUP_RANGE = 0x105,       //eq组的id内容 0x11
    EQ_ONLINE_CMD_MODE_SEQ_NUMBER = 0x106,      //mode的编号  magic num


    EQ_ONLINE_CMD_PARAMETER_SEG = 0x11,
    EQ_ONLINE_CMD_PARAMETER_TOTAL_GAIN,
    EQ_ONLINE_CMD_PARAMETER_LIMITER,
    EQ_ONLINE_CMD_PARAMETER_DRC,
    EQ_ONLINE_CMD_PARAMETER_CHANNEL,            //通道切换


    EQ_ONLINE_CMD_SONG_EQ_SEG = 0x2001,
    EQ_ONLINE_CMD_CALL_EQ_SEG = 0x2002,
    EQ_ONLINE_CMD_AEC_EQ_SEG = 0x2003,

    EQ_ONLINE_CMD_SONG_DRC = 0x2101,
//add xx here

    EQ_ONLINE_CMD_MAX,//最后一个
} EQ_ONLINE_CMD;


#if EQ_FADE_EN
static EQ_FADE_CFG *p_eq_fade_cfg = NULL;
#endif
static u8 eq_sectin_tab[] = {SONG_EQ_SECTION, CALL_EQ_SECTION, CALL_NARROW_EQ_SECTION, AEC_EQ_SECTION, AEC_NARROW_EQ_SECTION};

//每个模式的名字,使用utf8 编码格式填充,固定16个字节，不足16填 '\0'‘
#define song_mode_name          "普通音频EQ"
#define call_mode_name          "通话宽频下行EQ"
#define call_narrow_mode_name   "通话窄频下行EQ"
#define aec_mode_name           "通话宽频上行EQ"
#define aec_narrow_mode_name    "通话窄频上行EQ"

#define song_mode         0
#define call_mode         1
#define call_narrow_mode  2
#define aec_mode          3
#define aec_narrow_mode   4

//每个模式编号，可以用于文件存储数据模式识别
#define song_mode_seq         0x3000
#define call_mode_seq         0x3001
#define call_narrow_mode_seq  0x3002
#define aec_mode_seq          0x3003
#define aec_narrow_mode_seq   0x3004


//每个模式下，有多少组（有多少参数结构体）
#if TCFG_DRC_ENABLE
#define  song_mode_group_num 2    //普通音频eq drc
#else
#define  song_mode_group_num 1    //普通音频eq
#endif
#define  call_mode_group_num 1    //通话 eq
#define  aec_mode_group_num  1    //AEC eq


/*每个模式下拥有哪些 功能(group)*/
static u16 song_mode_groups[song_mode_group_num] = {
    EQ_ONLINE_CMD_SONG_EQ_SEG,
#if TCFG_DRC_ENABLE
    EQ_ONLINE_CMD_SONG_DRC,
#endif
};
static u16 call_mode_groups[call_mode_group_num] = {
    EQ_ONLINE_CMD_CALL_EQ_SEG,
};

static u16 aec_mode_groups[aec_mode_group_num] = {
    EQ_ONLINE_CMD_AEC_EQ_SEG,
};

static const int groups_num[] = {
    song_mode_group_num,
    call_mode_group_num,
    call_mode_group_num,
    aec_mode_group_num,
    aec_mode_group_num,
};

static const u16 *groups[] = {
    song_mode_groups,
    call_mode_groups,
    call_mode_groups,
    aec_mode_groups,
    aec_mode_groups,
};

static u16 *name[16] = {
    (u16 *)song_mode_name,
    (u16 *)call_mode_name,
    (u16 *)call_narrow_mode_name,
    (u16 *)aec_mode_name,
    (u16 *)aec_narrow_mode_name,
};

static u32 modes[] = {
    song_mode_seq,
    call_mode_seq,
    call_narrow_mode_seq,
    aec_mode_seq,
    aec_narrow_mode_seq,
};

void eq_section_num_set(u8 song, u8 call_16k, u8 call_8k, u8 aec_16k, u8 aec_8k)
{
    eq_sectin_tab[song_mode] = song;
    eq_sectin_tab[call_narrow_mode] = eq_sectin_tab[call_mode] = call_16k;//宽频、窄频eq段数需一致
    /* eq_sectin_tab[call_narrow_mode] = call_8k; */
    eq_sectin_tab[aec_narrow_mode] = eq_sectin_tab[aec_mode] = aec_16k;//宽频、窄频eq段数需一致
    /* eq_sectin_tab[aec_narrow_mode] = aec_8k; */
}

/*
 *eq类型的个数
 * */
static u16 groups_cnt[] = {EQ_ONLINE_CMD_SONG_EQ_SEG, EQ_ONLINE_CMD_CALL_EQ_SEG, EQ_ONLINE_CMD_CALL_EQ_SEG, EQ_ONLINE_CMD_AEC_EQ_SEG, EQ_ONLINE_CMD_AEC_EQ_SEG};

u8 get_index_by_id(u32 id)
{
    int i = 0;
    for (i = 0; i < ARRAY_SIZE(groups_cnt); i++) {
        if (groups_cnt[i] == id) {
            break;
        }
    }
    return i;
}

/*eq online packet*/
typedef struct {
    int cmd;     			///<EQ_ONLINE_CMD
    int data[45];       	///<data
} EQ_ONLINE_PACKET;



/*-----------------------------------------------------------*/
static u8 eq_mode = 0;
static EQ_CFG *p_eq_cfg = NULL;

static const EQ_CFG_SEG eq_seg_nor = {
    0, 2, 1000, 0 << 20, (int)(0.7f * (1 << 24))
};

/*-----------------------------------------------------------*/
static void eq_online_callback(uint8_t *packet, uint16_t size);
static void ci_send_packet_new(u32 id, u8 *packet, int size);
extern u16 crc_get_16bit(const void *src, u32 len);
float get_glbal_gain(EQ_CFG *eq_cfg, u8 mode);

int eq_seg_design(struct eq_seg_info *seg, int sample_rate, int *coeff)
{
    /* printf("seg:0x%x, coeff:0x%x, rate:%d, ", seg, coeff, sample_rate); */
    /* printf("idx:%d, iir:%d, freq:%d, gain:%d, q:%d ", seg->index, seg->iir_type, seg->freq, seg->gain, seg->q); */
    if (seg->freq >= (((u32)sample_rate / 2 * 29491) >> 15)) {
        log_w(" cur eq freq:%dHz not support sample_rate %dHz , so cur eq section set allpass \n", seg->freq, sample_rate);
        eq_get_AllpassCoeff(coeff);
        return false;
    }
    if (!seg->q || !seg->freq) {
        log_error("q err idx:%d, iir:%d, freq:%d, gain:%d, q:%d ", seg->index, seg->iir_type, seg->freq, seg->gain, seg->q);
        eq_get_AllpassCoeff(coeff);
        return false;
    }


    switch (seg->iir_type) {
    case EQ_IIR_TYPE_HIGH_PASS:
        design_hp(seg->freq, sample_rate, seg->q, coeff);
        break;
    case EQ_IIR_TYPE_LOW_PASS:
        design_lp(seg->freq, sample_rate, seg->q, coeff);
        break;
    case EQ_IIR_TYPE_BAND_PASS:
        design_pe(seg->freq, sample_rate, seg->gain, seg->q, coeff);
        break;
    case EQ_IIR_TYPE_HIGH_SHELF:
        design_hs(seg->freq, sample_rate, seg->gain, seg->q, coeff);
        break;
    case EQ_IIR_TYPE_LOW_SHELF:
        design_ls(seg->freq, sample_rate, seg->gain, seg->q, coeff);
        break;
    }
    int status = eq_stable_check(coeff);
    if (status) {
        log_error("eq_stable_check err:%d ", status);
        log_info("%d %d %d %d %d", coeff[0], coeff[1], coeff[2], coeff[3], coeff[4]);
        eq_get_AllpassCoeff(coeff);
        return false;
    }
    return true;
}

#if EQ_FADE_EN
static void eq_fade_tmr_deal(void *priv)
{
    EQ_FADE_CFG *eq_fade_cfg = priv;
    if (eq_fade_cfg && eq_fade_cfg->fade_stu) {
        eq_fade_cfg->fade_stu--;
    }
}
static void eq_fade_coeff_set(EQ_CFG *eq_cfg, int sr, u8 mode, EQ_CFG_SEG *seg, int *tar_tab, EQ_FADE_CFG *eq_fade_cfg)
{
    int i;
    if (!sr) {
        sr = 44100;
        log_error("sr is zero");
    }
    u16 cur_sr = eq_fade_cfg->sr[mode];
    for (i = 0; i < eq_cfg->seg_num[mode]; i++) {
        EQ_CFG_SEG *cur_seg = &eq_fade_cfg->seg[mode][i];
        EQ_CFG_SEG *use_seg = seg;
        u8 design = 0;
        local_irq_disable();
        if (cur_sr != sr) {
            /* printf("csr:%d, sr:%d \n", cur_sr, sr); */
            memcpy(cur_seg, use_seg, sizeof(EQ_CFG_SEG));
            eq_fade_cfg->fade_stu = 0;
            design = 1;
        } else {
            if (cur_seg->iir_type != use_seg->iir_type) {
                cur_seg->iir_type = use_seg->iir_type;
                design = 1;
            }
            if (cur_seg->freq != use_seg->freq) {
                cur_seg->freq = use_seg->freq;
                design = 1;
            }
            if (cur_seg->gain > use_seg->gain) {
                cur_seg->gain -= (int)(0.1f * (1 << 20));
                if (cur_seg->gain < use_seg->gain) {
                    cur_seg->gain = use_seg->gain;
                }
                design = 1;
            } else if (cur_seg->gain < use_seg->gain) {
                cur_seg->gain += (int)(0.1f * (1 << 20));
                if (cur_seg->gain > use_seg->gain) {
                    cur_seg->gain = use_seg->gain;
                }
                design = 1;
            }
            if (cur_seg->q > use_seg->q) {
                cur_seg->q -= (int)(0.1f * (1 << 24));
                if (cur_seg->q < use_seg->q) {
                    cur_seg->q = use_seg->q;
                }
                design = 1;
            } else if (cur_seg->q < use_seg->q) {
                cur_seg->q += (int)(0.1f * (1 << 24));
                if (cur_seg->q > use_seg->q) {
                    cur_seg->q = use_seg->q;
                }
                design = 1;
            }
            /* if ((cur_seg->gain != use_seg->gain) || (cur_seg->q != use_seg->q)) { */
            /* eq_fade_cfg->fade_stu = 2; */
            /* } */
        }
        local_irq_enable();
        if (design) {
            eq_fade_cfg->fade_stu++;
            /* printf("cg:%d, ug:%d, cq:%d, uq:%d \n", cur_seg->gain>>20, use_seg->gain>>20, cur_seg->q, use_seg->q); */
            /* int *tab = eq_cfg->song_mode_coeff_table[mode]; */
            eq_seg_design(cur_seg, sr, &tar_tab[5 * i]);
        }
    }
    eq_fade_cfg->sr[mode] = sr;
}

void *eq_fade_cfg_open()
{
    EQ_FADE_CFG *eq_fade_cfg = zalloc(sizeof(EQ_FADE_CFG));
    ASSERT(eq_fade_cfg);

    /* if (eq_fade_cfg->tmr == 0) { */
    /* eq_fade_cfg->tmr = sys_hi_timer_add(eq_fade_cfg, eq_fade_tmr_deal, 10); */
    /* } */
    for (int i = 0; i < eq_mode_num; i++) {
        /* eq_fade_cfg->global_gain[i] = 0.0; */
        for (int j = 0; j < EQ_SECTION_MAX; j++) {
            memcpy(&eq_fade_cfg->seg[i][j], &eq_seg_nor, sizeof(EQ_CFG_SEG));
        }
    }

    return eq_fade_cfg;
}

void eq_fade_cfg_close(EQ_FADE_CFG *eq_fade_cfg)
{
    if (eq_fade_cfg) {
        /* if (eq_fade_cfg->tmr) { */
        /* sys_hi_timer_del(eq_fade_cfg->tmr); */
        /* eq_fade_cfg->tmr = 0; */
        /* } */
        free(eq_fade_cfg);
        eq_fade_cfg = NULL;
    }
}
#endif

static void eq_coeff_set(EQ_CFG *eq_cfg, u16 sr, u8 mode, EQ_CFG_SEG *seg, int *tar_tab)
{
#ifndef CONFIG_EQ_NO_USE_COEFF_TABLE
#if EQ_FADE_EN
    if (p_eq_fade_cfg) {
        eq_fade_coeff_set(eq_cfg, sr, mode, seg, tar_tab, p_eq_fade_cfg);
        return ;
    }
#endif
    int i;
    if (!sr) {
        sr = 44100;
        log_error("sr is zero");
    }
    for (i = 0; i < eq_cfg->seg_num[mode]; i++) {
        if (eq_cfg->design_mask[mode] & BIT(i)) {
            eq_cfg->design_mask[mode] &= ~BIT(i);
            //printf("i =  %d %d\n", i, eq_cfg->seg_num[mode]);
            eq_seg_design(&seg[i], sr, &tar_tab[5 * i]);
            /*
                        printf("cf0:%d, cf1:%d, cf2:%d, cf3:%d, cf4:%d ", tar_tab[5 * i]
                               , tar_tab[5 * i + 1]
                               , tar_tab[5 * i + 2]
                               , tar_tab[5 * i + 3]
                               , tar_tab[5 * i + 4]
                              );
            */
        }
    }
#endif
}

void eq_seg_limit_zero(EQ_CFG_SEG *seg, u8 seg_num)
{
#if (RCSP_ADV_EN)&&(JL_EARPHONE_APP_EN)&&(TCFG_DRC_ENABLE == 0)
    int gain = 0;
    int cur_gain;
    for (int i = 0; i < seg_num; i++) {
        cur_gain = seg[i].gain >> 20;
        if (cur_gain > gain) {
            gain = cur_gain;
        }
    }
    if (!gain) {
        return ;
    }
    for (int i = 0; i < seg_num; i++) {
        cur_gain = seg[i].gain >> 20;
        cur_gain -= gain;
        seg[i].gain = cur_gain << 20;
    }
#endif
}

//================================================================================================================//
/*                                          EQ FILE DEAL                                                          */
/*                                                                                                                */
//================================================================================================================//
#if TCFG_EQ_FILE_ENABLE
static s32 eq_file_get_cfg(EQ_CFG *eq_cfg, u8 *path)
{
    u16 crc16 = 0;
    int ret = 0;
    FILE *file = NULL;
    u8 *file_data = NULL;
    u8 *head_buf = NULL;

    if (eq_cfg == NULL) {
        return  -EINVAL;
    }
    log_info(" %s\n", path);
    file = fopen((const char *)path, "r");
    if (file == NULL) {
        log_error("eq file open err\n");
        return  -ENOENT;
    }

    u8 fmt = 0;
    if (1 != fread(file, &fmt, 1)) {
        ret = -EIO;
        goto err_exit;
    }

    // eq ver
    u8 ver[4] = {0};
    if (4 != fread(file, ver, 4)) {
        ret = -EIO;
        goto err_exit;
    }
    if (memcmp(ver, audio_eq_ver, sizeof(audio_eq_ver))) {
        log_info("eq ver err \n");
        log_info_hexdump(ver, 4);
        fseek(file, 0, SEEK_SET);
        eq_cfg->eq_file_ver_err = 1;
        ret = -EIO;
        goto err_exit;

    }

    unsigned short mode_seq = 0;
    unsigned short mode_len = 0;
    u8 mode = 0;
    u8 mode_cnt = 0;
__next_mode:
    if (sizeof(unsigned short) != fread(file, &mode_seq, sizeof(unsigned short))) {
        ret = 0;
        goto err_exit;
    }
    if (sizeof(unsigned short) != fread(file, &mode_len, sizeof(unsigned short))) {
        ret = 0;
        goto err_exit;
    }
    int mode_start = fpos(file);
    int i = 0;
    for (i = 0; i < eq_mode_num; i++) {
        if (modes[i] == mode_seq) { //识别当前读到模式的序号
            mode = i;
            log_info("modes[%d] %x %x\n", i, modes[i], mode_seq);
            break;
        }
    }
    if (i == eq_mode_num) {
        ret = -EIO;
        goto err_exit;
    }


    while (1) {
        //read crc16
        if (sizeof(unsigned short) != fread(file, &crc16, sizeof(unsigned short))) {
            ret = 0;
            break;
        }
        int pos = fpos(file);
        //read id len
        EQ_FILE_HEAD *eq_file_h;
        int head_size = sizeof(EQ_FILE_HEAD);
        head_buf = malloc(head_size);
        if (sizeof(EQ_FILE_HEAD) != fread(file, head_buf, sizeof(EQ_FILE_HEAD))) {
            ret = -EIO;
            break;
        }
        eq_file_h = (EQ_FILE_HEAD *)head_buf;

        if ((eq_file_h->id >= EQ_ONLINE_CMD_SONG_EQ_SEG) && (eq_file_h->id < EQ_ONLINE_CMD_MAX)) {
            fseek(file, pos, SEEK_SET);
            int data_size = eq_file_h->len + sizeof(EQ_FILE_HEAD);
            file_data = malloc(data_size);
            if (file_data == NULL) {
                ret = -ENOMEM;
                break;
            }
            if (data_size != fread(file, file_data, data_size)) {
                ret = -EIO;
                break;
            }
            //compare crc16
            if (crc16 == crc_get_16bit(file_data, data_size)) {
                log_info("eq_file_h->id %x, %d\n", eq_file_h->id, mode);
                spin_lock(&eq_cfg->lock);

                EQ_CFG_SEG *seg = eq_cfg->cfg_parm[mode].song_eq_parm.parm.seg;
                int *tab = eq_cfg->song_mode_coeff_table[mode];
                if ((eq_file_h->id == EQ_ONLINE_CMD_SONG_EQ_SEG) ||
                    (eq_file_h->id == EQ_ONLINE_CMD_CALL_EQ_SEG) ||
                    (eq_file_h->id == EQ_ONLINE_CMD_AEC_EQ_SEG)) {
                    int size = sizeof(EQ_CFG_PARM);
                    int seg_num = 0;
                    if (data_size > size) { //防止段数大于 sdk 设置段数时，越界改写相邻drc结构数据
                        data_size = size;
                    }
                    memcpy(&eq_cfg->cfg_parm[mode].song_eq_parm, file_data, data_size);
                    seg_num = eq_cfg->cfg_parm[mode].song_eq_parm.parm.par.seg_num;
                    eq_cfg->design_mask[mode] = (u32) - 1;
                    if (seg_num > eq_sectin_tab[mode]) {
                        ret = -EIO;
                        log_error("song eq section err %d, %d\n", seg_num, eq_sectin_tab[mode]);
                        eq_cfg->eq_file_section_err = 1;
                        eq_cfg->cfg_parm[mode].song_eq_parm.parm.par.global_gain = 0;
                        spin_unlock(&eq_cfg->lock);
                        break;
                    }
                    eq_cfg->seg_num[mode] = seg_num;
                    float gain = eq_cfg->cfg_parm[mode].song_eq_parm.parm.par.global_gain;
                    put_buf(&gain, 4);
                    /* printf("eq_cfg->seg_num[%d] %d\n", mode, eq_cfg->seg_num[mode]); */
                    eq_coeff_set(eq_cfg, eq_cfg->cur_sr[mode], mode, seg, tab);
                    eq_cfg->online_updata[mode] = 1;
                } else if (eq_file_h->id == EQ_ONLINE_CMD_SONG_DRC) {
#if TCFG_DRC_ENABLE
                    memcpy(&eq_cfg->cfg_parm[mode].drc_parm, file_data, data_size);
                    eq_cfg->design_mask[mode] = (u32) - 1;
                    eq_cfg->drc_updata[mode] = 1;
#endif
                }
                spin_unlock(&eq_cfg->lock);

            } else {
                log_error("eq_cfg_info crc16 err\n");
                ret = -ENOEXEC;
                goto err_exit;
            }

            free(head_buf);
            head_buf = NULL;

            free(file_data);
            file_data = NULL;

        }

        int mode_end = fpos(file);
        if ((mode_end - mode_start) == mode_len) {
            goto __next_mode;
        }
    }

err_exit:
    if (head_buf) {
        free(head_buf);
        head_buf = NULL;
    }

    if (file_data) {
        free(file_data);
        file_data = NULL;
    }
    fclose(file);
    if (ret == 0) {
        log_info("cfg_info ok \n");
    }
    return ret;
}


/*
 *普通音频eq文件系数 回调
 * */
static int eq_file_get_filter_info(int sr, struct audio_eq_filter_info *info)
{
    int *coeff = NULL;
    if (!sr) {
        return -1;
    }
    u8 mode = song_mode;
    EQ_CFG *eq_cfg = p_eq_cfg;
    ASSERT(eq_cfg);
    EQ_CFG_SEG *seg = eq_cfg->cfg_parm[mode].song_eq_parm.parm.seg;
    int *tab = eq_cfg->song_mode_coeff_table[mode];
    if (sr != eq_cfg->cur_sr[mode]) {
        eq_cfg->cur_sr[mode] = sr;
        //更新coeff
        spin_lock(&eq_cfg->lock);
        eq_cfg->design_mask[mode] = (u32) - 1;
        eq_coeff_set(eq_cfg, sr, mode, seg, tab);
        spin_unlock(&eq_cfg->lock);
    }

#ifdef CONFIG_EQ_NO_USE_COEFF_TABLE
    coeff = seg;
    info->no_coeff = 1;
    info->sr = sr;
#else
    coeff = tab;
#endif

    info->L_coeff = info->R_coeff = (void *)coeff;
    info->L_gain = info->R_gain = get_glbal_gain(eq_cfg, mode);
    info->nsection = eq_cfg->seg_num[mode];
    return 0;
}
#endif

//================================================================================================================//
/*                                          EQ MODE DEAL                                                          */
/*                                                                                                                */
//================================================================================================================//
#if !TCFG_USE_EQ_FILE
static const EQ_CFG_SEG *eq_mode_tab[EQ_MODE_MAX] = {
    eq_tab_normal, eq_tab_rock, eq_tab_pop, eq_tab_classic, eq_tab_jazz, eq_tab_country, eq_tab_custom
};
#endif

__attribute__((weak)) u32 get_eq_mode_tab(void)
{

#if !TCFG_USE_EQ_FILE
    return (u32)eq_mode_tab;
#else
    return 0;
#endif
}

__attribute__((weak)) u8 get_eq_mode_max(void)
{
    return EQ_MODE_MAX;
}

#if (EQ_SECTION_MAX==9)
static const eq_mode_use_idx[] = {
    0,	1,	2,	3,	4,	5,	/*6,*/	7,	8,	9
};
#elif (EQ_SECTION_MAX==8)
static const eq_mode_use_idx[] = {
    0,	/*1,*/	2,	3,	4,	5,	6,	7,	/*8,*/	9
};
#elif (EQ_SECTION_MAX==7)
static const eq_mode_use_idx[] = {
    0,	/*1,*/	2,	3,	4,	5,	/*6,*/	7,	/*8,*/	9
};
#elif (EQ_SECTION_MAX==6)
static const eq_mode_use_idx[] = {
    0,	/*1,*/	2,	3,	4,	/*5,*/	/*6,*/	7,	/*8,*/	9
};
#elif (EQ_SECTION_MAX==5)
static const eq_mode_use_idx[] = {
    /*0,*/	1,	/*2,*/	3,	/*4,*/	5,	/*6,*/	7,	/*8,*/	9
};
#else
static const eq_mode_use_idx[] = {
    0,	1,	2,	3,	4,	5,	6,	7,	8,	9
};
#endif
/*
 *无eq_cfg_hw.bin时，使用系统默认eq配置表
 * */
static int eq_mode_get_filter_info(int sr, struct audio_eq_filter_info *info)
{
#if !TCFG_USE_EQ_FILE
    int *coeff = NULL;
    if (!sr) {
        return -1;
    }
    EQ_CFG *eq_cfg = p_eq_cfg;
    ASSERT(eq_cfg);

    u8 mode = song_mode;
    EQ_CFG_SEG *seg = eq_cfg->cfg_parm[mode].song_eq_parm.parm.seg;
    int *tab = eq_cfg->song_mode_coeff_table[mode];
    int *tmp = (int *)get_eq_mode_tab();
    EQ_CFG_SEG *eq_tab = (EQ_CFG_SEG *)tmp[eq_mode];

#if 0
    memcpy(seg, (void *)eq_tab[eq_mode], sizeof(EQ_CFG_SEG)*eq_cfg->seg_num[mode] /* EQ_SECTION_MAX */);
#else

    for (int j = 0; j < eq_cfg->seg_num[mode]; j++) {
        memcpy(&seg[j], &eq_tab[eq_mode_use_idx[j]], sizeof(EQ_CFG_SEG));
    }

#endif
    eq_seg_limit_zero(seg, eq_cfg->seg_num[mode]);

    spin_lock(&eq_cfg->lock);
    eq_cfg->design_mask[mode] = (u32) - 1;
    eq_coeff_set(eq_cfg, sr, mode, seg, tab);
#if EQ_FADE_EN
    if (p_eq_fade_cfg->fade_stu) {
        p_eq_fade_cfg->fade_stu--;
    }
    if (p_eq_fade_cfg && p_eq_fade_cfg->fade_stu) {
        ;
    } else
#endif
    {
        eq_cfg->mode_updata = 0;
    }
    spin_unlock(&eq_cfg->lock);

#ifdef CONFIG_EQ_NO_USE_COEFF_TABLE
    coeff = seg;
    info->no_coeff = 1;
    info->sr = sr;
#else
    coeff = tab;
#endif

    eq_cfg->cur_sr[mode] = 0;

    info->L_coeff = info->R_coeff = (void *)coeff;
#if (RCSP_ADV_EN)&&(JL_EARPHONE_APP_EN)&&(TCFG_DRC_ENABLE == 0)
    info->L_gain = info->R_gain = -11.3f;
#else
    info->L_gain = info->R_gain = get_glbal_gain(eq_cfg, mode);
#endif
    info->nsection = eq_cfg->seg_num[mode];
#endif
    return 0;
}

/*
 *获取各段eq增益
 * */
s8 eq_get_tab_gain(u8 mode, u8 index)
{
    /* EQ_CFG_SEG *mode_tab = (EQ_CFG_SEG *)(get_eq_mode_tab() + mode * sizeof(EQ_CFG_SEG *)); */

    int *tmp = (int *)get_eq_mode_tab();
    EQ_CFG_SEG *mode_tab = (EQ_CFG_SEG *)tmp[mode];

    return (s8)(mode_tab[index].gain >> 20);
}

/*
 *设置各段eq增益
 * */
void eq_set_tab_gain(u8 mode, s8 *gain, u8 section)
{
    u8 i;
    /* EQ_CFG_SEG *mode_tab = (EQ_CFG_SEG *)(get_eq_mode_tab() + mode * sizeof(EQ_CFG_SEG *)); */

    int *tmp = (int *)get_eq_mode_tab();
    EQ_CFG_SEG *mode_tab = (EQ_CFG_SEG *)tmp[mode];

    for (i = 0; i < section; i++) {
        mode_tab[i].gain = gain[i] << 20;
    }
}

/*
 *获取 eq总增益
 * */
float get_glbal_gain(EQ_CFG *eq_cfg, u8 mode)
{
    float  gain = 0;
    gain = eq_cfg->cfg_parm[mode].song_eq_parm.parm.par.global_gain;
    return gain;
}

/*
 *设置eq 总增益
 * **/
void set_global_gain(EQ_CFG *eq_cfg, u8 mode, float global_gain)
{
    eq_cfg->cfg_parm[mode].song_eq_parm.parm.par.global_gain = global_gain;
}

/*
 *设置eq 总增益
 * **/
void eq_global_gain_set(float global_gain)
{
    EQ_CFG *eq_cfg = p_eq_cfg;
    u8 mode = song_mode;
    if (p_eq_cfg) {
        CFG_PARM *cfg = &eq_cfg->cfg_parm[mode].song_eq_parm.parm.par;
        cfg->global_gain = global_gain;
    }
}

/*
 *设置eq type
 * **/
void eq_type_set(u8 type)
{
    if (p_eq_cfg) {
        p_eq_cfg->eq_type = type;
    }
}

/*
 *设置eq section num
 * **/
void eq_seg_num_set(u8 seg_num)
{
    u8 mode = song_mode;
    if (p_eq_cfg) {
        p_eq_cfg->seg_num[mode] = seg_num;
    }
}


/*
 *设置eq mode
 * **/
int eq_mode_set(u8 mode)
{
#if !TCFG_USE_EQ_FILE
    if (mode >= get_eq_mode_max()) {
        mode = 0;
    }
    eq_mode = mode;
    log_info("set eq mode %d\n", eq_mode);

    if (p_eq_cfg && ((p_eq_cfg->eq_type == EQ_TYPE_MODE_TAB) || (p_eq_cfg->eq_type == EQ_TYPE_FILE))) {
        p_eq_cfg->mode_updata = 1;
    }
#endif
    return 0;
}

/*
 *设置eq mode switch
 * **/
int eq_mode_sw(void)
{
#if !TCFG_USE_EQ_FILE
    eq_mode++;
    if (eq_mode >= get_eq_mode_max()) {
        eq_mode = 0;
    }
    log_info("sw eq mode %d\n", eq_mode);

    if (p_eq_cfg && (p_eq_cfg->eq_type == EQ_TYPE_MODE_TAB)) {
        p_eq_cfg->mode_updata = 1;
    }
#endif
    return 0;
}

/*
 *获取eq mode
 * **/
int eq_mode_get_cur(void)
{
    return  eq_mode;
}

/*
 *custom mode set
 * **/
int eq_mode_set_custom_param(u16 index, int gain)
{
#if !TCFG_USE_EQ_FILE
    u16 i;
    if (index >= EQ_SECTION_MAX) {
        return 0;
    }

    eq_tab_custom[eq_mode_use_idx[index]].gain = gain << 20;
    log_info("set custom eq param %d\n", gain);
#endif
    return 0;
}

/*
 *custom mode param get
 * **/
s8 eq_mode_get_gain(u8 mode, u16 index)
{
#if !TCFG_USE_EQ_FILE
    u16 i;
    if (mode >= get_eq_mode_max()) {
        return 0;
    }
    if (index >= EQ_SECTION_MAX) {
        return 0;
    }

    u8 val = 0;
    /* EQ_CFG_SEG *eq_tab = (EQ_CFG_SEG *)(get_eq_mode_tab() + mode * sizeof(EQ_CFG_SEG *)); */
    int *tmp = (int *)get_eq_mode_tab();
    EQ_CFG_SEG *eq_tab = (EQ_CFG_SEG *)tmp[mode];

    val = eq_tab[eq_mode_use_idx[index]].gain >> 20;
    log_info("get custom eq param %d\n", val);
    return val;
#endif
    return 0;
}

/*
 *eq mode freq get
 * **/
int eq_mode_get_freq(u8 mode, u16 index)
{
#if !TCFG_USE_EQ_FILE
    u16 i;
    if (mode >= get_eq_mode_max()) {
        return 0;
    }
    if (index >= EQ_SECTION_MAX) {
        return 0;
    }
    int val = 0;
    /* EQ_CFG_SEG *eq_tab = (EQ_CFG_SEG *)(get_eq_mode_tab() + mode * sizeof(EQ_CFG_SEG *)); */
    int *tmp = (int *)get_eq_mode_tab();
    EQ_CFG_SEG *eq_tab = (EQ_CFG_SEG *)tmp[mode];
    val = eq_tab[eq_mode_use_idx[index]].freq;
    log_info("get custom eq freq %d\n", val);

    return val;
#endif
    return 0;
}

//================================================================================================================//
/*                                          EQ debug online                                                       */
/*                                                                                                                */
//================================================================================================================//
#if TCFG_EQ_ONLINE_ENABLE
#include "config/config_interface.h"

#if TCFG_USER_TWS_ENABLE
extern void *tws_ci_sync_open(void *priv, void (*rx_handler)(void *, void *, int));
extern int tws_ci_data_sync(void *priv, void *data, int len, u32 time);
/* static u16 pack_eq_len[] = {12 + 10 * 16, 10 * 16, 12 * 16}; //EQ_CFG_PARAMETER 分包4包，每包长度,最后一包是 drc */
#if (EQ_SECTION_MAX <= 10)
static u16 pack_eq_len[] = {12 +  EQ_SECTION_MAX * 16};
#elif ((EQ_SECTION_MAX > 10) && (EQ_SECTION_MAX  <= 20))
static u16 pack_eq_len[] = {12 + 10 * 16, (EQ_SECTION_MAX - 10) * 16};
#else
static u16 pack_eq_len[] = {12 + 10 * 16, 10 * 16, (EQ_SECTION_MAX - 10) * 16}; //EQ_CFG_PARAMETER 分包4包，每包长度,最后一包是 drc
#endif

static void eq_online_tws_send_tmr(void *priv)
{
    if (!p_eq_cfg) {
        return ;
    }
    /* printf("send:"); */
    /* printf_buf(&p_eq_cfg->param, sizeof(EQ_CFG_PARAMETER)); */

    /*同步串口收到数据到对耳*/
#if (EQ_SECTION_MAX >= 5)
    EQ_CFG *eq_cfg = p_eq_cfg;
    u8 mode = eq_cfg->mode_id;
    u8 id = eq_cfg->pack_id;
    u16 len = 0;
    eq_cfg->tws_pack[0] = id;
    eq_cfg->tws_pack[1] = mode;
    if (id < ARRAY_SIZE(pack_eq_len)) {
        u8 *parm = &eq_cfg->cfg_parm[mode].song_eq_parm.parm;
        u32 offset = 0;
        for (int i = 0; i < id; i++) {
            offset += pack_eq_len[i];
        }

        len = pack_eq_len[id] + 6;
        log_d("len %d\n", len);
        eq_cfg->tws_pack[2] = len;
        memcpy(&eq_cfg->tws_pack[3], &parm[offset], len);
    } else {
        u8 *parm = &eq_cfg->cfg_parm[mode].drc_parm;
        len = sizeof(DRC_CFG_PARM) + 6;
        eq_cfg->tws_pack[2] = len;
        memcpy(&eq_cfg->tws_pack[3], parm, len);
    }

    tws_ci_data_sync(eq_cfg->tws_ci, eq_cfg->tws_pack, len, 0/*ms*/);

#else

    u8 mode = p_eq_cfg->mode_id;
    void *parm = NULL;
    p_eq_cfg->cfg_parm[mode].cur_mode = mode;
    parm = &p_eq_cfg->cfg_parm[mode];
    tws_ci_data_sync(p_eq_cfg->tws_ci, parm, sizeof(EQ_FILE_PARM), 0/*ms*/);
#endif
}

static void eq_online_tws_updata(void *priv)
{
    if (!p_eq_cfg) {
        return ;
    }
    eq_online_tws_send_tmr(NULL);
    u8 mode = p_eq_cfg->mode_id;
    p_eq_cfg->online_updata[mode] = 1;
    p_eq_cfg->drc_updata[mode] = 1;
}

static void eq_online_tws_send(void)
{
    if (!p_eq_cfg) {
        return ;
    }
    spin_lock(&p_eq_cfg->lock);
    if (!p_eq_cfg->tws_tmr) {
        p_eq_cfg->tws_tmr = sys_timer_add(NULL, eq_online_tws_send_tmr, 500);
    }
    spin_unlock(&p_eq_cfg->lock);
}

static void eq_online_tws_ci_data_rx_handler(void *priv, void *data, int len)
{
    if (!p_eq_cfg) {
        return ;
    }
    spin_lock(&p_eq_cfg->lock);
    if (p_eq_cfg->tws_tmr) { // pc调试端
#if (EQ_SECTION_MAX >= 5)
        EQ_CFG *eq_cfg = p_eq_cfg;
        u16 *get_tws_pack = data;
        u8 id = get_tws_pack[0];
        u8 mode = get_tws_pack[1];
        u16 len = get_tws_pack[2] - 6;
        u32 offset = 0;
        u8 *parm = NULL;
        if (id < ARRAY_SIZE(pack_eq_len)) {
            parm = &eq_cfg->cfg_parm[mode].song_eq_parm.parm;
            for (int i = 0; i < id; i++) {
                offset += pack_eq_len[i];
            }
        } else {
            parm = &eq_cfg->cfg_parm[mode].drc_parm;
        }
        log_d("tx check\n");
        if (0 == memcmp(&get_tws_pack[3], &parm[offset], len)) {
            log_d("tx check ok\n");
            // 数据相同，结束
            sys_timer_del(eq_cfg->tws_tmr);
            eq_cfg->tws_tmr = 0;
        }

#else

        u8 mode = p_eq_cfg->mode_id;
        //printf("txxxxx %d\n", mode);
        if (0 == memcmp(data, &p_eq_cfg->cfg_parm[mode], sizeof(EQ_FILE_PARM))) {
            // 数据相同，结束
            sys_timer_del(p_eq_cfg->tws_tmr);
            p_eq_cfg->tws_tmr = 0;
        }
#endif
    } else { // tws接受端
#if (EQ_SECTION_MAX >= 5)
        log_d("rx \n");
        EQ_CFG *eq_cfg = p_eq_cfg;
        u16 *get_tws_pack = data;
        u8 id = get_tws_pack[0];
        u8 mode = get_tws_pack[1];
        u16 len = get_tws_pack[2] - 6;
        eq_cfg->mode_id = mode;
        eq_cfg->pack_id = id;
        if (id < ARRAY_SIZE(pack_eq_len)) {
            u8 *parm = &eq_cfg->cfg_parm[mode].song_eq_parm.parm;
            u32 offset = 0;
            for (int i = 0; i < id; i++) {
                offset += pack_eq_len[i];
            }
            memcpy(&parm[offset], &get_tws_pack[3], len);
            eq_cfg->design_mask[mode] = (u32) - 1;
        } else {
            u8 *parm = &eq_cfg->cfg_parm[mode].drc_parm;
            len = sizeof(DRC_CFG_PARM) - 6;
            memcpy(parm, &get_tws_pack[3], len);
        }

        sys_timeout_add(eq_cfg, eq_online_tws_updata, 10);
#else

        EQ_FILE_PARM cfg_parm_tmp = {0};
        memcpy(&cfg_parm_tmp, data, sizeof(EQ_FILE_PARM));
        u8 mode = cfg_parm_tmp.cur_mode;
        p_eq_cfg->mode_id = mode;
        //printf("rxxxxxx %d\n", mode);
        memcpy(&p_eq_cfg->cfg_parm[mode], data, sizeof(EQ_FILE_PARM));
        p_eq_cfg->design_mask[mode] = (u32) - 1;
        sys_timeout_add(NULL, eq_online_tws_updata, 10);
#endif
    }
    spin_unlock(&p_eq_cfg->lock);
}

static void eq_online_tws_open(void)
{
    p_eq_cfg->tws_ci = tws_ci_sync_open(NULL, eq_online_tws_ci_data_rx_handler);
#if (EQ_SECTION_MAX >= 5)
    if (p_eq_cfg->tws_ci) {
        p_eq_cfg->tws_pack = zalloc(200);
    }
#endif

}

static void eq_online_tws_close(void)
{
    spin_lock(&p_eq_cfg->lock);
    if (p_eq_cfg->tws_tmr) { // pc调试端
        sys_timer_del(p_eq_cfg->tws_tmr);
        p_eq_cfg->tws_tmr = 0;
    }
    spin_unlock(&p_eq_cfg->lock);
    tws_ci_sync_open(NULL, NULL);
#if (EQ_SECTION_MAX >= 5)
    spin_lock(&p_eq_cfg->lock);
    if (p_eq_cfg->tws_pack) {
        free(p_eq_cfg->tws_pack);
        p_eq_cfg->tws_pack = NULL;
    }
#endif
    spin_unlock(&p_eq_cfg->lock);

}
#endif

/*
 *eq drc 在线调试，系数解析函数
 * */
static s32 eq_online_update(EQ_CFG *eq_cfg, EQ_ONLINE_PACKET *packet)
{
    EQ_ONLINE_PARAMETER_SEG seg;
    log_i("eq_cmd:0x%x ", packet->cmd);
    if (eq_cfg->eq_type != EQ_TYPE_ONLINE) {
        return -EPERM;
    }
    u32 mode_id = 0;
    switch (packet->cmd) {
    case EQ_ONLINE_CMD_SONG_EQ_SEG:
    case EQ_ONLINE_CMD_CALL_EQ_SEG:
    case EQ_ONLINE_CMD_AEC_EQ_SEG:
        spin_lock(&eq_cfg->lock);
        memcpy(&seg, &packet->data[1], sizeof(EQ_ONLINE_PARAMETER_SEG));
        mode_id = packet->data[0];
        eq_cfg->cfg_parm[mode_id].cur_mode = mode_id ;
        eq_cfg->mode_id = mode_id;
        if (seg.index == (u16) - 1) {
            float global_gain;
            memcpy(&global_gain, &seg.iir_type, sizeof(float));
            set_global_gain(eq_cfg, mode_id, global_gain);
            //put_buf(&global_gain, 4);
        }

        if (seg.index >= eq_cfg->seg_num[mode_id]) {
            if (seg.index != (u16) - 1) {
                log_error("index:%d ", seg.index);
            }
#if (TCFG_EQ_ONLINE_ENABLE && TCFG_USER_TWS_ENABLE)
            if (!eq_cfg->tws_tmr) {
                eq_cfg->pack_id = 0;
            }
#endif

            spin_unlock(&eq_cfg->lock);
            break;
        }
        memcpy(&eq_cfg->cfg_parm[mode_id].song_eq_parm.parm.seg[seg.index], &seg, sizeof(EQ_ONLINE_PARAMETER_SEG));

        eq_cfg->design_mask[mode_id] |= BIT(seg.index);
#if (TCFG_EQ_ONLINE_ENABLE && TCFG_USER_TWS_ENABLE)
        if (!eq_cfg->tws_tmr) {
            if (seg.index < 10) {
                eq_cfg->pack_id = 0;
            } else if ((seg.index < 20)) {
                eq_cfg->pack_id = 1;
            } else {
                eq_cfg->pack_id = 2;
            }
        }
#endif

        spin_unlock(&eq_cfg->lock);

        /* log_info("idx:%d, iir:%d, frq:%d, gain:%d, q:%d \n", seg.index, seg.iir_type, seg.freq, seg.gain, seg.q); */
        break;

    case EQ_ONLINE_CMD_SONG_DRC:
        /* log_info("EQ_ONLINE_CMD_PARAMETER_DRC"); */
#if TCFG_DRC_ENABLE
        spin_lock(&eq_cfg->lock);
#if (TCFG_EQ_ONLINE_ENABLE && TCFG_USER_TWS_ENABLE)
        if (!eq_cfg->tws_tmr) {
            eq_cfg->pack_id = 3;
        }
#endif

        memcpy(&eq_cfg->cfg_parm[mode_id].drc_parm.parm.drc, &packet->data[1], sizeof(struct drc_ch_org));
        mode_id = packet->data[0];
        eq_cfg->cfg_parm[mode_id].cur_mode = mode_id ;
        eq_cfg->mode_id = mode_id;
        spin_unlock(&eq_cfg->lock);
#endif
        break;
    default:
        return -EINVAL;
    }

#if TCFG_USER_TWS_ENABLE
    eq_online_tws_send();
#endif

    int cmd = packet->cmd;
    spin_lock(&eq_cfg->lock);
    if (cmd == EQ_ONLINE_CMD_SONG_DRC) {
#if TCFG_DRC_ENABLE
        eq_cfg->drc_updata[mode_id] = 1;
#endif
    } else {
        eq_cfg->online_updata[mode_id] = 1;
    }
    spin_unlock(&eq_cfg->lock);

    return 0;
}

static int eq_online_nor_cmd(EQ_CFG *eq_cfg, EQ_ONLINE_PACKET *packet)
{
    if (eq_cfg->eq_type != EQ_TYPE_ONLINE) {
        return -EPERM;
    }
    u32 id = EQ_CONFIG_ID;
    if (packet->cmd == EQ_ONLINE_CMD_GETVER) {
        struct eq_ver_info {
            char sdkname[16];
            u8 eqver[4];
        };
        struct eq_ver_info eq_ver_info = {0};
        memcpy(eq_ver_info.sdkname, audio_eq_sdk_name, sizeof(audio_eq_sdk_name));
        memcpy(eq_ver_info.eqver, audio_eq_ver, sizeof(audio_eq_ver));
        /* printf("audio_eq_ver %s\n", audio_eq_ver); */
        put_buf(audio_eq_ver, sizeof(audio_eq_ver));
        ci_send_packet_new(EQ_CONFIG_ID, (u8 *)&eq_ver_info, sizeof(struct eq_ver_info));
        return 0;
    } else if (packet->cmd == EQ_ONLINE_CMD_GET_SECTION_NUM || (packet->cmd == EQ_EQ_ONLINE_CMD_GET_SECTION_NUM)) {
        struct _cmd {
            int id;
            int groupId;
        };
        struct _cmd cmd = {0};

        memcpy(&cmd, packet->data, sizeof(struct _cmd));
        uint8_t hw_section = eq_sectin_tab[get_index_by_id(cmd.id)];
        ci_send_packet_new(id, (u8 *)&hw_section, sizeof(uint8_t));
        return 0;
    } else if (packet->cmd == EQ_ONLINE_CMD_GLOBAL_GAIN_SUPPORT_FLOAT) {
        uint8_t support_float = 1;
        ci_send_packet_new(id, (u8 *)&support_float, sizeof(uint8_t));
        return 0;
    } else if (packet->cmd == EQ_ONLINE_CMD_PASSWORD) {
        uint8_t password = 0;
        if (password_en) {
            password = 1;
        }
        ci_send_packet_new(id, (u8 *)&password, sizeof(uint8_t));
        return 0;
    } else if (packet->cmd == EQ_ONLINE_CMD_VERIFY_PASSWORD) {
        //check password
        int len = 0;
        char pass[64];
        typedef struct password {
            int len;
            char pass[45];
        } PASSWORD;

        PASSWORD ps = {0};
        spin_lock(&eq_cfg->lock);
        memcpy(&ps, packet->data, sizeof(PASSWORD));
        memcpy(&ps, packet->data, sizeof(int) + ps.len);
        spin_unlock(&eq_cfg->lock);

        uint8_t password_ok = 0;
        if (!strcmp(ps.pass, audio_eq_password)) {
            password_ok = 1;
        } else {
            log_error("password verify failxx \n");
        }
        eq_cfg->password_ok = password_ok;
        ci_send_packet_new(id, (u8 *)&password_ok, sizeof(uint8_t));
        return 0;
    } else if (packet->cmd == EQ_ONLINE_CMD_FILE_SIZE) {
        if (password_en) {
            if (!eq_cfg->password_ok) {
                log_error("pass not verify\n");
                return -EINVAL;
            }
        }
        struct file_s {
            int id;
            int fileid;
        };
        struct file_s fs;
        memcpy(&fs, packet, sizeof(struct file_s));
        /* if (fs.fileid != 0x1) { */
        /* log_error("fs.fileid %d\n", fs.fileid); */
        /* return -EINVAL; */
        /* } */
        if ((fs.fileid != 0x1) && (fs.fileid != 0x11)) {
            log_error("fs.fileid 0x%x\n", fs.fileid);
            return 0;
        }

        u32 file_size  = 0;
        if (fs.fileid == 0x1) {
            FILE *file = NULL;
            file = fopen(EQ_FILE_NAME, "r");
            if (!file) {
                log_error("EQ_FILE_NAME err %s\n", EQ_FILE_NAME);
            } else {
                file_size = flen(file);
                fclose(file);
            }
            if (eq_cfg->eq_file_ver_err == 1) {
                file_size = 0;
                log_error("eq ver err \n");
            }
            if (eq_cfg->eq_file_section_err == 1) {
                file_size = 0;
                log_error("eq section err \n");
            }
        } else if ((fs.fileid == 0x11)) {
            /* if (eq_cfg->stero) { */
            /* file_size = sizeof(drc_constriants); */
            /* } */
        }

        ci_send_packet_new(id, (u8 *)&file_size, sizeof(u32));
        return 0;
    } else if (packet->cmd == EQ_ONLINE_CMD_FILE) {

        if (password_en) {
            if (!eq_cfg->password_ok) {
                log_error("pass not verify\n");
                return -EINVAL;
            }
        }

        struct file_s {
            int id;
            int fileid;
            int offset;
            int size;
        };
        struct file_s fs;
        memcpy(&fs, packet, sizeof(struct file_s));
        /* if (fs.fileid != 0x1) { */
        /* log_error("fs.fileid %d\n", fs.fileid); */
        /* return -EINVAL; */
        /* } */
        if ((fs.fileid != 0x1) && (fs.fileid != 0x11)) {
            log_error("fs.fileid 0x%x\n", fs.fileid);
            return 0;
        }

        if (fs.fileid == 0x1) {
            FILE *file = NULL;
            file = fopen(EQ_FILE_NAME, "r");
            if (!file) {
                return -EINVAL;
            }

            if (eq_cfg->eq_file_ver_err == 1) {
                fclose(file);
                log_error("eq ver err \n");
                return -EINVAL;
            }
            if (eq_cfg->eq_file_section_err == 1) {
                fclose(file);
                log_error("eq section err \n");
                return -EINVAL;
            }

            fseek(file, fs.offset, SEEK_SET);
            u8 *data = malloc(fs.size);
            if (!data) {
                fclose(file);
                return -EINVAL;
            }
            int ret = fread(file, data, fs.size);
            if (ret != fs.size) {
            }
            fclose(file);
            ci_send_packet_new(id, (u8 *)data, fs.size);
            free(data);
        } else if (fs.fileid == 0x11) {
            ci_send_packet_new(id, (u8 *)drc_constriants, fs.size);
        }

        return 0;
    } else if (packet->cmd == EQ_ONLINE_CMD_MODE_COUNT) {
        //模式个数
        int mode_cnt = eq_mode_num;
        ci_send_packet_new(id, (u8 *)&mode_cnt, sizeof(int));
        return 0;
    } else if (packet->cmd == EQ_ONLINE_CMD_MODE_NAME) {
        //utf8编码得名字
        struct cmd {
            int id;
            int modeId;
        };
        struct cmd cmd;
        memcpy(&cmd, packet, sizeof(struct cmd));

        u8 tmp[32] = {0};
        u8 name_len =  strlen(name[cmd.modeId]);
        //log_i("%s, cmd.modeId %d, name_len %d\n", name[cmd.modeId], cmd.modeId, name_len);
        memcpy(tmp, name[cmd.modeId], name_len);
        ci_send_packet_new(id, (u8 *)tmp, name_len);
        return 0;
    } else if (packet->cmd == EQ_ONLINE_CMD_MODE_GROUP_COUNT) {
        struct cmd {
            int id;
            int modeId;
        };
        struct cmd cmd;
        memcpy(&cmd, packet, sizeof(struct cmd));

        ci_send_packet_new(id, (u8 *)&groups_num[cmd.modeId], sizeof(int));
        return 0;

    } else if (packet->cmd == EQ_ONLINE_CMD_MODE_GROUP_RANGE) { //摸下是组的id
        struct cmd {
            int id;
            int modeId;
            int offset;
            int count;
        };
        struct cmd cmd;
        memcpy(&cmd, packet, sizeof(struct cmd));

        u16 *group_tmp = groups[cmd.modeId];
        ci_send_packet_new(id, (u8 *)&group_tmp[cmd.offset], cmd.count * sizeof(u16));
        return 0;
    } else if (packet->cmd == EQ_ONLINE_CMD_EQ_GROUP_COUNT) {
        u32 eq_group_num = ARRAY_SIZE(groups_cnt);
        ci_send_packet_new(id, (u8 *)&eq_group_num, sizeof(u32));

        return 0;
    } else if (packet->cmd == EQ_ONLINE_CMD_EQ_GROUP_RANGE) {
        struct cmd {
            int id;
            int offset;
            int count;
        };
        struct cmd cmd;
        memcpy(&cmd, packet, sizeof(struct cmd));

        u16 g_id[32];
        memcpy(g_id, &groups_cnt[cmd.offset], cmd.count * sizeof(u16));
        ci_send_packet_new(id, (u8 *)&g_id[cmd.offset], cmd.count * sizeof(u16));

        return 0;
    } else if (packet->cmd == EQ_EQ_ONLINE_CMD_CHANGE_MODE) {
        struct cmd {
            int id;
            int modeId;
        };
        struct cmd cmd;
        memcpy(&cmd, packet, sizeof(struct cmd));
        for (int i = 0; i < 3; i++) {
            eq_cfg->online_updata[i] = 0;
            eq_cfg->drc_updata[i] = 0;
        }

        eq_cfg->mode_id = cmd.modeId;
        log_info("========eq_cfg->mode_id  %d\n", eq_cfg->mode_id);
        ci_send_packet_new(id, (u8 *)"OK", 2);
        return 0;
    } else if (packet->cmd == EQ_ONLINE_CMD_MODE_SEQ_NUMBER) {
        struct cmd {
            int id;
            int modeId;
        };
        struct cmd cmd;
        memcpy(&cmd, packet, sizeof(struct cmd));

        ci_send_packet_new(id, (u8 *)&modes[cmd.modeId], sizeof(u32));

        return 0;
    } else if (packet->cmd == EQ_ONLINE_CMD_INQUIRE) { //pc 查询小鸡是否在线
        ci_send_packet_new(id, (u8 *)"NO", 2);//OK表示需要重传，NO表示不需要重传,ER还是表示未知命令
        return 0;
    }

    return -EINVAL;
}

static void eq_online_callback_new_protocol(uint8_t *packet, uint16_t size)
{
    u8 *ptr = packet;
    reply_sq = ptr[1];
    reply_to_tool =  0;
    u8 *new_packet = (void *)&packet[2];
    eq_online_callback(new_packet, size);
}

static void eq_online_callback_spp(uint8_t *packet, uint16_t size)
{
    ASSERT(((int)packet & 0x3) == 0, "buf %x size %d\n", packet, size);
    eq_online_callback(packet, size);
}
static void eq_online_callback(uint8_t *packet, uint16_t size)
{
    s32 res;
    EQ_CFG *eq_cfg = p_eq_cfg;
    if (!eq_cfg) {
        return ;
    }

    ASSERT(((int)packet & 0x3) == 0, "buf %x size %d\n", packet, size);
    res = eq_online_update(eq_cfg, (EQ_ONLINE_PACKET *)packet);
    /* log_info_hexdump(packet, sizeof(EQ_ONLINE_PACKET)); */

    u32 id = EQ_CONFIG_ID;

    if (res == 0) {
        log_info("Ack");
        ci_send_packet_new(id, (u8 *)"OK", 2);
    } else {
        res = eq_online_nor_cmd(eq_cfg, (EQ_ONLINE_PACKET *)packet);
        if (res == 0) {
            return ;
        }
        /* log_info("Nack"); */
        ci_send_packet_new(id, (u8 *)"ER", 2);
    }
}

/*
 *在线eq系数回调
 * */
static int eq_online_get_filter_info(int sr, struct audio_eq_filter_info *info)
{
    int *coeff = NULL;
    if (!sr) {
        return -1;
    }
    EQ_CFG *eq_cfg = p_eq_cfg;
    ASSERT(eq_cfg);
    u8 mode = AUDIO_SONG_EQ_NAME;//eq_cfg->mode_id;
    EQ_CFG_SEG *seg = eq_cfg->cfg_parm[mode].song_eq_parm.parm.seg;
    int *tab = eq_cfg->song_mode_coeff_table[mode];

    /* log_i("eq_cfg->online_updata[%d] %d\n", mode, eq_cfg->online_updata[mode]); */
    /* log_i("sr %d %d\n", sr, eq_cfg->cur_sr[mode]); */
    if ((sr != eq_cfg->cur_sr[mode]) || (eq_cfg->online_updata[mode])) {
        //在线请求coeff
        spin_lock(&eq_cfg->lock);
        if (sr != eq_cfg->cur_sr[mode]) {
            eq_cfg->design_mask[mode] = (u32) - 1;
        }
        eq_coeff_set(eq_cfg, sr, mode, seg, tab);
        spin_unlock(&eq_cfg->lock);
        eq_cfg->cur_sr[mode] = sr;
#if EQ_FADE_EN
        if (p_eq_fade_cfg->fade_stu) {
            p_eq_fade_cfg->fade_stu--;
        }

        if (p_eq_fade_cfg && p_eq_fade_cfg->fade_stu) {
            ;
        } else
#endif
        {
            eq_cfg->online_updata[mode] = 0;
        }
    }

#ifdef CONFIG_EQ_NO_USE_COEFF_TABLE
    coeff = seg;
    info->no_coeff = 1;
    info->sr = sr;
#else
    coeff = tab;
#endif

    info->L_coeff = info->R_coeff = (void *)coeff;
    info->L_gain = info->R_gain = get_glbal_gain(eq_cfg, mode);
    info->nsection = eq_cfg->seg_num[mode];
    return 0;
}

static int eq_online_open(void)
{
    int i = 0;
    int j = 0;
    EQ_CFG *eq_cfg = p_eq_cfg;
    spin_lock(&eq_cfg->lock);
#if 0
    for (i = 0; i < eq_mode_num; i++) {
        eq_cfg->seg_num[i] = eq_sectin_tab[i];
        eq_cfg->design_mask[i] = (u32) - 1;
        u8 mode = i;
        log_i("p_eq_cfg->eq_type %x\n", p_eq_cfg->eq_type);
        if (p_eq_cfg->eq_type != EQ_TYPE_FILE) { //没有默认配置文件时，才去设置默认的系数
            u16 sr = eq_cfg->cur_sr[mode];
            for (j = 0; j < eq_cfg->seg_num[i]; j++) {
                memcpy(&eq_cfg->cfg_parm[mode].song_eq_parm.parm.seg[j], &eq_seg_nor, sizeof(EQ_CFG_SEG));
            }
            EQ_CFG_SEG *seg = eq_cfg->cfg_parm[mode].song_eq_parm.parm.seg;
            eq_coeff_set(eq_cfg, sr, mode, seg, eq_cfg->song_mode_coeff_table[mode]);
        }
    }
#endif
    eq_cfg->eq_type = EQ_TYPE_ONLINE;
    //printf("cccccccip_eq_cfg->eq_type %x\n", p_eq_cfg->eq_type);
    spin_unlock(&eq_cfg->lock);

#if TCFG_USER_TWS_ENABLE
    eq_online_tws_open();
#endif
    return 0;
}
static void eq_online_close(void)
{
#if TCFG_USER_TWS_ENABLE
    eq_online_tws_close();
#endif
}

//旧调音注册
REGISTER_CONFIG_TARGET(eq_config_target) = {
    .id         = EQ_CONFIG_ID,
    .callback   = eq_online_callback,
};
//新调音注册
REGISTER_DETECT_TARGET(eq_adj_target) = {
    .id         = EQ_CONFIG_ID,
    .tool_message_deal   = eq_online_callback_new_protocol,
};

//EQ在线调试不进power down
static u8 eq_online_idle_query(void)
{
    if (!p_eq_cfg) {
        return 1;
    }
    return 0;
}

REGISTER_LP_TARGET(eq_online_lp_target) = {
    .name = "eq_online",
    .is_idle = eq_online_idle_query,
};
#endif /*TCFG_EQ_ONLINE_ENABLE*/

//================================================================================================================//
/*                                          通话上行EQ DEAL                                                       */
/*                                                                                                                */
//================================================================================================================//
#if TCFG_AEC_UL_EQ_ENABLE
#if !TCFG_USE_EQ_FILE
const struct eq_seg_info ul_eq_tab_normal[] = {
    {0, EQ_IIR_TYPE_HIGH_PASS, 200,   0 << 20, (int)(0.7f  * (1 << 24))},
    {1, EQ_IIR_TYPE_BAND_PASS, 300,   0 << 20, (int)(0.7f  * (1 << 24))},
    {2, EQ_IIR_TYPE_BAND_PASS, 400,   0 << 20, (int)(0.7f  * (1 << 24))},
    {3, EQ_IIR_TYPE_BAND_PASS, 450,   0 << 20, (int)(0.7f  * (1 << 24))},
    {4, EQ_IIR_TYPE_BAND_PASS, 500,   0 << 20, (int)(0.7f  * (1 << 24))},
    {5, EQ_IIR_TYPE_BAND_PASS, 1000,  0 << 20, (int)(0.7f  * (1 << 24))},
    {6, EQ_IIR_TYPE_BAND_PASS, 2000,  0 << 20, (int)(0.7f  * (1 << 24))},
    {7, EQ_IIR_TYPE_BAND_PASS, 4000,  0 << 20, (int)(0.7f  * (1 << 24))},
    {8, EQ_IIR_TYPE_BAND_PASS, 8000,  0 << 20, (int)(0.7f  * (1 << 24))},
    {9, EQ_IIR_TYPE_BAND_PASS, 16000, 0 << 20, (int)(0.7f  * (1 << 24))},
};
#endif

/*
 *aec eq系数 回调
 * */
int aec_ul_eq_filter(int sr, struct audio_eq_filter_info *info)
{
    int *coeff = NULL;
    if (!sr) {
        return -1;
    }
    EQ_CFG *eq_cfg = p_eq_cfg;
    ASSERT(eq_cfg);
    u8 mode = aec_mode;
    if (sr == 8000) {
        mode = aec_narrow_mode;
    }
    if ((sr != 8000) && (!eq_cfg->online_updata[mode]) && eq_cfg->online_updata[aec_narrow_mode]) {
        mode = aec_narrow_mode;
    }
    log_i("aec_ul_eq_filter mode %d, sr %d\n", mode, sr);
    EQ_CFG_SEG *seg = eq_cfg->cfg_parm[mode].song_eq_parm.parm.seg;
    int *tab = eq_cfg->song_mode_coeff_table[mode];

#if !TCFG_USE_EQ_FILE
    if (p_eq_cfg->eq_type == EQ_TYPE_MODE_TAB) {
        if (eq_cfg->seg_num[mode] > ARRAY_SIZE(ul_eq_tab_normal)) {
            eq_cfg->seg_num[mode] = ARRAY_SIZE(ul_eq_tab_normal);
        }
        memcpy(seg, ul_eq_tab_normal, sizeof(EQ_CFG_SEG)*eq_cfg->seg_num[mode]);
    }
#endif

    if ((sr != eq_cfg->cur_sr[mode]) || (eq_cfg->online_updata[mode])) {
        eq_cfg->cur_sr[mode] = sr;
        //更新coeff
        spin_lock(&eq_cfg->lock);
        eq_cfg->design_mask[mode] = (u32) - 1;
        eq_coeff_set(eq_cfg, sr, mode, seg, tab);
        spin_unlock(&eq_cfg->lock);
        /* eq_cfg->online_updata[mode] = 0; */
    }
    eq_cfg->online_updata[aec_mode] = 0;
    eq_cfg->online_updata[aec_narrow_mode] = 0;

#ifdef CONFIG_EQ_NO_USE_COEFF_TABLE
    coeff = seg;
    info->no_coeff = 1;
    info->sr = sr;
#else
    coeff = tab;
#endif

    info->L_coeff = info->R_coeff = (void *)coeff;
    info->L_gain = info->R_gain = get_glbal_gain(eq_cfg, mode);
    info->nsection = eq_cfg->seg_num[mode];
    return 0;
}
#endif

//================================================================================================================//
/*                                          通话下行EQ DEAL                                                       */
/*                                                                                                                */
//================================================================================================================//
#if TCFG_PHONE_EQ_ENABLE
#if !TCFG_USE_EQ_FILE
const struct eq_seg_info phone_eq_tab_normal[] = {
    {0, EQ_IIR_TYPE_HIGH_PASS, 200,   0 << 20, (int)(0.7f  * (1 << 24))},
    {1, EQ_IIR_TYPE_BAND_PASS, 300,   0 << 20, (int)(0.7f  * (1 << 24))},
    {2, EQ_IIR_TYPE_BAND_PASS, 400,   0 << 20, (int)(0.7f  * (1 << 24))},
    {3, EQ_IIR_TYPE_BAND_PASS, 450,   0 << 20, (int)(0.7f  * (1 << 24))},
    {4, EQ_IIR_TYPE_BAND_PASS, 500,   0 << 20, (int)(0.7f  * (1 << 24))},
    {5, EQ_IIR_TYPE_BAND_PASS, 1000,  0 << 20, (int)(0.7f  * (1 << 24))},
    {6, EQ_IIR_TYPE_BAND_PASS, 2000,  0 << 20, (int)(0.7f  * (1 << 24))},
    {7, EQ_IIR_TYPE_BAND_PASS, 4000,  0 << 20, (int)(0.7f  * (1 << 24))},
    {8, EQ_IIR_TYPE_BAND_PASS, 8000,  0 << 20, (int)(0.7f  * (1 << 24))},
    {9, EQ_IIR_TYPE_BAND_PASS, 16000, 0 << 20, (int)(0.7f  * (1 << 24))},
};
#endif

/*
 *通话eq系数 的回调函数。eq_config.c中 重定义时，可支持使用eq_cfg_hw.bin文件读取系数
 * */
int eq_phone_get_filter_info(int sr, struct audio_eq_filter_info *info)
{
    int *coeff = NULL;
    if (!sr) {
        return -1;
    }
    EQ_CFG *eq_cfg = p_eq_cfg;
    ASSERT(eq_cfg);
    u8 mode = call_mode;
    if (sr == 8000) {
        mode = call_narrow_mode;
    }
    if ((sr != 8000) && (!eq_cfg->online_updata[mode]) && eq_cfg->online_updata[call_narrow_mode]) {
        mode = call_narrow_mode;
    }

    log_i("eq_phone_get_filter_info mode %d, sr%d\n", mode, sr);

    EQ_CFG_SEG *seg = eq_cfg->cfg_parm[mode].song_eq_parm.parm.seg;
    int *tab = eq_cfg->song_mode_coeff_table[mode];
#if !TCFG_USE_EQ_FILE
    if (p_eq_cfg->eq_type == EQ_TYPE_MODE_TAB) {
        if (eq_cfg->seg_num[mode] > ARRAY_SIZE(phone_eq_tab_normal)) {
            eq_cfg->seg_num[mode] = ARRAY_SIZE(phone_eq_tab_normal);
        }
        memcpy(seg, phone_eq_tab_normal, sizeof(EQ_CFG_SEG)*eq_cfg->seg_num[mode]);
    }
#endif

    if ((sr != eq_cfg->cur_sr[mode]) || (eq_cfg->online_updata[mode])) {
        eq_cfg->cur_sr[mode] = sr;
        //更新coeff
        spin_lock(&eq_cfg->lock);
        eq_cfg->design_mask[mode] = (u32) - 1;
        eq_coeff_set(eq_cfg, sr, mode, seg, tab);
        spin_unlock(&eq_cfg->lock);
        //eq_cfg->online_updata[mode] = 0;
    }
    eq_cfg->online_updata[call_mode] = 0;
    eq_cfg->online_updata[call_narrow_mode] = 0;


#ifdef CONFIG_EQ_NO_USE_COEFF_TABLE
    coeff = seg;
    info->no_coeff = 1;
    info->sr = sr;
#else
    coeff = tab;
#endif

    info->L_coeff = info->R_coeff = (void *)coeff;
    info->L_gain = info->R_gain = get_glbal_gain(eq_cfg, mode);
    info->nsection = eq_cfg->seg_num[mode];

    return 0;
}
#endif


//================================================================================================================//
/*                                          播歌EQ DEAL                                                       */
/*                                                                                                                */
//================================================================================================================//
int eq_get_filter_info(int sr, struct audio_eq_filter_info *info)
{
    u8 mode = p_eq_cfg->mode_id;
    log_info("%s, sr:%d, cur sr:%d, type:%d \n", __func__, sr, p_eq_cfg->cur_sr[mode], p_eq_cfg->eq_type);
    switch (p_eq_cfg->eq_type) {
    case EQ_TYPE_MODE_TAB:
        return eq_mode_get_filter_info(sr, info);
#if TCFG_EQ_FILE_ENABLE
    case EQ_TYPE_FILE:
        return eq_file_get_filter_info(sr, info);
#endif
#if TCFG_EQ_ONLINE_ENABLE
    case EQ_TYPE_ONLINE:
        return eq_online_get_filter_info(sr, info);
#endif
    }
    return -1;
}

void eq_get_cfg_info(struct audio_eq_cfg_info *cfg)
{
    ASSERT(p_eq_cfg);
}

/*-----------------------------------------------------------*/
int drc_get_filter_info(struct audio_drc_filter_info *info)
{
#if TCFG_DRC_ENABLE
    EQ_CFG *eq_cfg = p_eq_cfg;
    ASSERT(eq_cfg);
    u8 mode = p_eq_cfg->mode_id;
    p_eq_cfg->drc_updata[mode] = 0;

    info->pch = info->R_pch = &eq_cfg->cfg_parm[mode].drc_parm.parm.drc;
#endif
    return 0;
}

/*-----------------------------------------------------------*/
// app
void eq_app_run_check(struct audio_eq *eq)
{
    ASSERT(p_eq_cfg);
#if EQ_FADE_EN
    if (p_eq_fade_cfg && p_eq_fade_cfg->fade_stu) {
        return ;
    }
#endif

#if TCFG_EQ_ONLINE_ENABLE
    if ((p_eq_cfg->online_updata[aec_mode] || p_eq_cfg->online_updata[aec_narrow_mode]) && eq->online_en && (eq->eq_name == AUDIO_AEC_EQ_NAME)) {
#if TCFG_AEC_UL_EQ_ENABLE
        eq->cb = aec_ul_eq_filter;
#endif
        eq->updata = 1;
    } else if ((p_eq_cfg->online_updata[call_mode] || p_eq_cfg->online_updata[call_narrow_mode]) && eq->online_en && (eq->eq_name == AUDIO_CALL_EQ_NAME)) {
#if TCFG_PHONE_EQ_ENABLE
        eq->cb = eq_phone_get_filter_info;
#endif
        eq->updata = 1;
    } else if (p_eq_cfg->online_updata[song_mode] && eq->online_en && (eq->eq_name == AUDIO_SONG_EQ_NAME)) {
        eq->cb = eq_online_get_filter_info;
        eq->updata = 1;
    }
#endif
    if (p_eq_cfg->mode_updata && eq->mode_en) {
        r_printf("%s, type: %d, mode: %d", __func__, p_eq_cfg->eq_type, eq_mode);
        p_eq_cfg->mode_updata = 0;
        /* eq->cb = eq_mode_get_filter_info; */
        eq->cb = eq_get_filter_info;
        eq->updata = 1;
    }
}

AT(.audio_drc.text.cache.L2.drc)
void drc_app_run_check(struct audio_drc *drc)
{
#if TCFG_DRC_ENABLE
    ASSERT(p_eq_cfg);

    if (p_eq_cfg->drc_updata[song_mode] && drc->online_en) {
        if ((drc->drc_name == 1)) {
        } else if ((drc->drc_name == 0)) {
            p_eq_cfg->drc_updata[song_mode] = 0;
            drc->cb = drc_get_filter_info;
            drc->updata = 1;
        }
    }
#endif
}
void eq_cfg_default_init(EQ_CFG *eq_cfg)
{
    for (int i = 0; i < eq_mode_num; i++) {
        eq_cfg->cur_sr[i] = 44100;
    }

    for (int i = 0; i < eq_mode_num; i++) {
        eq_cfg->seg_num[i] = eq_sectin_tab[i];
        eq_cfg->design_mask[i] = (u32) - 1;
        u8 mode = i;
        /* printf("eq_cfg->eq_type %x\n", eq_cfg->eq_type); */
        if (eq_cfg->eq_type != EQ_TYPE_FILE) {
            u16 sr = eq_cfg->cur_sr[mode];
            for (int j = 0; j < eq_cfg->seg_num[i]; j++) {
                memcpy(&eq_cfg->cfg_parm[mode].song_eq_parm.parm.seg[j], &eq_seg_nor, sizeof(EQ_CFG_SEG));
            }
            EQ_CFG_SEG *seg = eq_cfg->cfg_parm[mode].song_eq_parm.parm.seg;
            int *tab = eq_cfg->song_mode_coeff_table[mode];
            eq_coeff_set(eq_cfg, sr, mode, seg, tab);
        }

#if TCFG_DRC_ENABLE
        eq_cfg->cfg_parm[i].drc_parm.parm.drc.nband = 1;
        eq_cfg->cfg_parm[i].drc_parm.parm.drc.type = 1;
        eq_cfg->cfg_parm[i].drc_parm.parm.drc._p.limiter[0].attacktime = 5;
        eq_cfg->cfg_parm[i].drc_parm.parm.drc._p.limiter[0].releasetime = 300;
        int th = 0;//db
        eq_cfg->cfg_parm[i].drc_parm.parm.drc._p.limiter[0].threshold[0] = round(pow(10.0,  th / 20.0) * 32768); // 32768; // 0db:32768, -60db:33
        eq_cfg->cfg_parm[i].drc_parm.parm.drc._p.limiter[0].threshold[1] = 32768;
#endif
    }

}
/*-----------------------------------------------------------*/
void eq_cfg_open(void)
{
    if (p_eq_cfg == NULL) {
        p_eq_cfg = zalloc(sizeof(EQ_CFG));
        ASSERT(p_eq_cfg);
    }
    eq_cfg_default_init(p_eq_cfg);

    spin_lock_init(&p_eq_cfg->lock);
#if TCFG_EQ_FILE_ENABLE
    p_eq_cfg->eq_type = EQ_TYPE_FILE;
    if (eq_file_get_cfg(p_eq_cfg, EQ_FILE_NAME)) //获取EQ文件失败
#endif
    {
#if !TCFG_USE_EQ_FILE
        p_eq_cfg->eq_type = EQ_TYPE_MODE_TAB;
#if !TCFG_EQ_ONLINE_ENABLE
        for (int i = 0; i < eq_mode_num; i++) {
            /* p_eq_cfg->seg_num[i] = eq_sectin_tab[i]; */
            if (p_eq_cfg->seg_num[i] > EQ_SECTION_MAX_DEFAULT) {
                p_eq_cfg->seg_num[i] = EQ_SECTION_MAX_DEFAULT;
            }
        }
#endif
#endif
    }

#if TCFG_EQ_ONLINE_ENABLE
    eq_online_open();
#endif

#if EQ_FADE_EN
    if (!p_eq_fade_cfg) {
        p_eq_fade_cfg = eq_fade_cfg_open();
    }
#endif

}

void eq_cfg_close(void)
{
    if (!p_eq_cfg) {
        return ;
    }

#if TCFG_EQ_ONLINE_ENABLE
    eq_online_close();
#endif

    void *ptr = p_eq_cfg;
    p_eq_cfg = NULL;
    free(ptr);

#if EQ_FADE_EN
    eq_fade_cfg_close(p_eq_fade_cfg);
#endif
}

static u8 parse_seq = 0;
extern void ci_send_packet(u32 id, u8 *packet, int size);
static void ci_send_packet_new(u32 id, u8 *packet, int size)
{
#if APP_ONLINE_DEBUG
    if (DB_PKT_TYPE_EQ == id) {
        app_online_db_ack(parse_seq, packet, size);
    }
#else
    if (get_uart_protocol()) {
        all_assemble_package_send_to_pc(reply_to_tool, reply_sq, packet, size);
    } else {
        ci_send_packet(id, packet, size);
    }
#endif/*APP_ONLINE_DEBUG*/
}

/*
 *手机app 在线调时，数据解析的回调
 * */
static int eq_app_online_parse(u8 *packet, u8 size, u8 *ext_data, u16 ext_size)
{
#if TCFG_EQ_ONLINE_ENABLE
    parse_seq = ext_data[1];
    eq_online_callback_spp(packet, size);
#else
    log_i("EQ_ONLINE,not enable!\n");
#endif
    return 0;
}

extern u32 EQ_PRIV_SECTION_NUM;
int eq_init(void)
{
    eq_cfg_open();
    int cpu_section = 0;
    if (EQ_SECTION_MAX > 10) {
        cpu_section = EQ_SECTION_MAX - (int)&EQ_PRIV_SECTION_NUM;
    }

#if defined(TCFG_AUDIO_BASS_BOOST) && TCFG_AUDIO_BASS_BOOST
    cpu_section +=  4;
#endif
    audio_eq_init(cpu_section);

#if APP_ONLINE_DEBUG
    app_online_db_register_handle(DB_PKT_TYPE_EQ, eq_app_online_parse);
#endif
    return 0;
}
__initcall(eq_init);

#endif

