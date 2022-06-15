#include "asm/includes.h"
#include "media/includes.h"
#include "system/includes.h"
#include "app_config.h"
#include "circular_buf.h"
#include "audio_iis.h"
#include "audio_link.h"
#include "audio_syncts.h"

#if TCFG_AUDIO_OUTPUT_IIS
#define IIS_TX_BUF_LEN 	(2*35000/(1000000/TCFG_IIS_SAMPLE_RATE)/8*8)  //cbuf缓冲35ms, 中断缓冲10ms左右
struct _iis_output {
    u32 in_sample_rate;
    u32 out_sample_rate;
    cbuffer_t cbuf;
    u8 buf[IIS_TX_BUF_LEN];
    u8 tbuf[1024];
    struct audio_src_handle *hw_src;
    u16 sr_cal_timer;
    u32 points_total;

    volatile u32 pre_clkn;
    volatile u32 pre_finecnt;

    volatile u32 cur_clkn;
    volatile u32 cur_finecnt;
    volatile u32 start_clkn;
    volatile u32 start_finecnt;
    volatile u32 irq_cnt;
    volatile u32 irq_cnt_tar;
    volatile u32 iis_time_gap;
    volatile u32 iis_time_sum;
    volatile u32 iis_time_cnt;

    volatile u32 dabuf;
    volatile float daspace;
    u8 ch;
    u8 flag;
    u8 syncts_en;
    u8 state;
    struct list_head sync_list;
} ;
struct _iis_output *iis_output_hdl = NULL;
static u32 iis_srI_last = 0;

u8 audio_iis_is_working()
{
    if (!iis_output_hdl) {
        return 0;
    }
    local_irq_disable();
    if (iis_output_hdl->state) {
        local_irq_enable();
        return 1;
    }
    local_irq_enable();

    return 0;
}

int audio_iis_irq_resume_decoder(int time_ms, void *priv, void (*callback)(void *))
{
    sys_hi_timeout_add(priv, callback, time_ms);
    return 0;
}

int outsize_dabuf()
{
    int rlen = 0;
    local_irq_disable();
    if (iis_output_hdl) {
        rlen = iis_output_hdl->dabuf;
    }
    local_irq_enable();
    return (rlen);//points
}

__attribute__((weak))
void local_bt_us_time(u32 *_clkn, u32 *finecnt)
{
    printf(" test a2dp get us time\n");
}

int audio_iis_buffered_frames()
{
    //iis 2ch
    if (iis_output_hdl) {
        u32 cur_clkn = 0;
        u32 cur_finecnt = 0;
        int time_gap = 0;

        local_irq_disable();
        if (iis_output_hdl->state == 2) {
            local_bt_us_time(&cur_clkn, &cur_finecnt);
            time_gap = ((cur_clkn - iis_output_hdl->pre_clkn) & 0x7ffffff) * 625 + (cur_finecnt - iis_output_hdl->pre_finecnt);
            time_gap = (iis_output_hdl->iis_time_gap - time_gap);//cbuf_read了，正在的用的那块，未消耗完的部分
            time_gap +=	iis_output_hdl->iis_time_gap;//cbuf_read 了未开始用的那一块
            time_gap /= iis_output_hdl->daspace;// frames
        }
        local_irq_enable();
        return time_gap + (cbuf_get_data_len(&(iis_output_hdl->cbuf)) >> 2);

    }
    return 0;
}

int audio_iis_data_time()
{
    int dac_buffered_time = 0;
    if (iis_output_hdl) {
        dac_buffered_time = ((audio_iis_buffered_frames() * 1000000) / TCFG_IIS_SAMPLE_RATE) / 1000;
    }
    return  dac_buffered_time;
}

sec(.volatile_ram_code)
int outsize_outsr_ok()
{
    if (iis_output_hdl && (iis_output_hdl->irq_cnt > 1)) {
        return 0;
    }
    return -1;
}
//iis从机实际采样率 相比16000，误差在%6左右，此处让同步模块兼容阈值设置程%10
u16 iis_resample_out_step = 1000;
#define  iis_cnt_threadhold  (100)
#define  iis_normal_gap    (ALNK_BUF_POINTS_NUM*1000000/TCFG_IIS_SAMPLE_RATE + 5000) //iis中断间隔 us
#define  iis_tar_gap       (ALNK_BUF_POINTS_NUM*1000000/TCFG_IIS_SAMPLE_RATE)
sec(.volatile_ram_code)
void adjust_iis_us_time()
{
    local_bt_us_time(&iis_output_hdl->cur_clkn, &iis_output_hdl->cur_finecnt);
    u32 time_gap = ((iis_output_hdl->cur_clkn - iis_output_hdl->pre_clkn) & 0x7ffffff) * 625 + (iis_output_hdl->cur_finecnt - iis_output_hdl->pre_finecnt);
    if (iis_output_hdl->iis_time_cnt > iis_cnt_threadhold) {
        if ((time_gap > (iis_output_hdl->iis_time_gap + 5)) /* || (time_gap < (iis_output_hdl->iis_time_gap - 1)) */)  {//大于才做调整
            u32 clkn = iis_output_hdl->cur_clkn;
            u32 finecnt = iis_output_hdl->cur_finecnt;
            iis_output_hdl->cur_clkn = iis_output_hdl->pre_clkn + iis_output_hdl->iis_time_gap / 625;
            iis_output_hdl->cur_finecnt = iis_output_hdl->pre_finecnt + iis_output_hdl->iis_time_gap % 625;
            iis_output_hdl->cur_clkn += iis_output_hdl->cur_finecnt / 625;
            iis_output_hdl->cur_finecnt %= 625;
            iis_output_hdl->pre_clkn = clkn;
            iis_output_hdl->pre_finecnt = finecnt;

            /* if (iis_output_hdl->cur_finecnt > 625){ */
            /* printf("iis clkn %d, finecnt %d, pre_clkn %d pre_finecnt %d %d\n",  */
            /* iis_output_hdl->cur_clkn,iis_output_hdl->cur_finecnt, iis_output_hdl->pre_clkn, iis_output_hdl->pre_finecnt, iis_output_hdl->iis_time_gap); */
            /* } */
            return;
        }
    }

    if (iis_output_hdl->irq_cnt == iis_cnt_threadhold) {
        iis_output_hdl->iis_time_cnt = iis_cnt_threadhold + 1;
        iis_output_hdl->iis_time_gap = (((iis_output_hdl->cur_clkn - iis_output_hdl->start_clkn) & 0x7ffffff) * 625 + (iis_output_hdl->cur_finecnt -  iis_output_hdl->start_finecnt)) / iis_cnt_threadhold;
    } else if (iis_output_hdl->irq_cnt < iis_cnt_threadhold) {
        if (time_gap < iis_normal_gap) {
            iis_output_hdl->iis_time_gap = time_gap;
        }
    }
    iis_output_hdl->pre_clkn = iis_output_hdl->cur_clkn;
    iis_output_hdl->pre_finecnt = iis_output_hdl->cur_finecnt;
    iis_output_hdl->state = 2;
    /* printf("iis_output_hdl->iis_time_cnt %d\n", iis_output_hdl->iis_time_cnt); */
    /* printf("gap %d  %d us cnt %d\n", time_gap, iis_output_hdl->iis_time_gap, iis_output_hdl->iis_time_cnt); */
    /* local_bt_us_time(&iis_output_hdl->pre_clkn, &iis_output_hdl->pre_finecnt); */
}

//iis中断调用，计算daspace
sec(.volatile_ram_code)
static float calc_daspace(u32 len)
{
    adjust_iis_us_time();
    float daspace = iis_output_hdl->daspace;
    if (iis_output_hdl->irq_cnt == 0) {
        iis_output_hdl->start_clkn = iis_output_hdl->cur_clkn;
        iis_output_hdl->start_finecnt = iis_output_hdl->cur_finecnt;
        iis_output_hdl->irq_cnt++;
    } else if (iis_output_hdl->irq_cnt < iis_output_hdl->irq_cnt_tar) {
        daspace = (float)(((iis_output_hdl->cur_clkn - iis_output_hdl->start_clkn) & 0x7ffffff) * 625 + (iis_output_hdl->cur_finecnt -  iis_output_hdl->start_finecnt)) / (float)((len >> 2) * (iis_output_hdl->irq_cnt)); //time/points/ch_num
        iis_output_hdl->irq_cnt++;
    }
    /* printf("daspace %d.%d", (int)daspace, (int)((daspace - (int)daspace)*1000000)); */
    return daspace;
}

sec(.volatile_ram_code)
void local_iis_us_time(u32 *clkn, u32 *finecnt)
{
    if (iis_output_hdl) {
        *clkn = iis_output_hdl->cur_clkn;
        *finecnt = iis_output_hdl->cur_finecnt;
    }
}
sec(.volatile_ram_code)
float outsize_daspace()//us
{
    if (iis_output_hdl) {
        return iis_output_hdl->daspace;
    }
    return (float)1000000 / (float)TCFG_IIS_SAMPLE_RATE;
    /* return 22.6f;//采样间隔 */
}

int audio_iis_src_output_handler(void *parm, s16 *data, int len)
{
    int wlen = cbuf_write(&(iis_output_hdl->cbuf), data, len);
    return wlen;
}

int audio_iis_output_write(s16 *data, u32 len)
{
    u32 wlen = 0;
    if (iis_output_hdl) {
        if (iis_output_hdl->hw_src) {
            wlen = audio_src_resample_write(iis_output_hdl->hw_src, data, len);
        } else {
            if (iis_output_hdl->syncts_en) {
                local_irq_disable();
                wlen = cbuf_write(&(iis_output_hdl->cbuf), data, len);
                u16 frames = wlen >> 2; //len/2/ch
                iis_output_hdl->dabuf += frames;
                local_irq_enable();
                audio_iis_count_to_syncts(frames);
            } else {
                wlen = cbuf_write(&(iis_output_hdl->cbuf), data, len);
            }
        }
    }
    return wlen;
}

sec(.volatile_ram_code)
void audio_iis_tx_isr(s16 *data, u32 len)
{
    u32 wlen = 0;
    u32 rlen = 0;
    if (iis_output_hdl) {
        iis_output_hdl->points_total += len >> 1;
        if (iis_output_hdl->syncts_en) {
            local_irq_disable();
            iis_output_hdl->daspace = calc_daspace(len);
            rlen = cbuf_get_data_len(&(iis_output_hdl->cbuf));
            iis_output_hdl->dabuf = rlen >> 2;
            local_irq_enable();
        }

        /* if ((iis_output_hdl->flag == 0) && (rlen < (IIS_TX_BUF_LEN / 2))) { */
        /* memset(data, 0x00, len); */
        /* return; */
        /* } */
        iis_output_hdl->flag = 1;

        if (rlen >= len) {
            wlen = cbuf_read(&(iis_output_hdl->cbuf), data, len);
        } else {
            printf(" iis end\n");
            wlen = cbuf_read(&(iis_output_hdl->cbuf), data, rlen);
            memset((u8 *)data + wlen, 0x00, len - wlen);
            iis_output_hdl->flag = 0;
        }
    }
}

void audio_iis_sr_cal_timer(void *param)
{
    if (iis_output_hdl) {
        iis_output_hdl->out_sample_rate = iis_output_hdl->points_total >> 1;
        iis_output_hdl->points_total = 0;
        printf("audio_iis_sr_cal_timer o: (%d, %d)\n", iis_output_hdl->in_sample_rate, iis_output_hdl->out_sample_rate);
        if (iis_output_hdl->hw_src) {
            audio_hw_src_set_rate(iis_output_hdl->hw_src, \
                                  iis_output_hdl->in_sample_rate, \
                                  iis_output_hdl->out_sample_rate);
        }
    }
}

void audio_iis_output_set_srI(u32 sample_rate)
{
    printf("audio_iis_output_set_srI %d\n", sample_rate);
    iis_srI_last = sample_rate;
    if (iis_output_hdl) {
        iis_output_hdl->in_sample_rate = sample_rate;
        if (iis_output_hdl->hw_src) {
            audio_hw_src_set_rate(iis_output_hdl->hw_src, \
                                  iis_output_hdl->in_sample_rate, \
                                  iis_output_hdl->out_sample_rate);
        }
    }
}

void audio_iis_output_start(u8 ch)
{
    printf("audio_iis_output_start\n");
    if (iis_output_hdl == NULL) {
        iis_output_hdl = zalloc(sizeof(struct _iis_output));
        if (iis_output_hdl == NULL) {
            printf("audio_iis_output_start malloc err !\n");
            return;
        }
        clk_set("sys", 96 * 1000000L);
        clk_set_en(0);
        cbuf_init(&(iis_output_hdl->cbuf), iis_output_hdl->buf, IIS_TX_BUF_LEN);
        iis_output_hdl->in_sample_rate = iis_srI_last;
        iis_output_hdl->out_sample_rate = TCFG_IIS_SAMPLE_RATE;
        iis_output_hdl->ch = ch;
        iis_output_hdl->syncts_en = 1;//syncts_en;
        if (!iis_output_hdl->syncts_en) {
            iis_output_hdl->sr_cal_timer = sys_hi_timer_add(NULL, audio_iis_sr_cal_timer, 1000);
            iis_output_hdl->hw_src = zalloc(sizeof(struct audio_src_handle));
        }

        if (iis_output_hdl->hw_src) {
            audio_hw_src_open(iis_output_hdl->hw_src, 2, SRC_TYPE_RESAMPLE);
            audio_hw_src_set_rate(iis_output_hdl->hw_src, \
                                  iis_output_hdl->in_sample_rate, \
                                  iis_output_hdl->out_sample_rate);
            audio_src_set_output_handler(iis_output_hdl->hw_src, NULL, audio_iis_src_output_handler);
        } else {
            printf("hw_src malloc err ==============\n");
        }

        audio_link_init();
        alink_channel_init(ch, ALINK_DIR_TX, audio_iis_tx_isr);
        cbuf_write(&(iis_output_hdl->cbuf), iis_output_hdl->buf, (IIS_TX_BUF_LEN / 2));
        INIT_LIST_HEAD(&iis_output_hdl->sync_list);
        iis_output_hdl->daspace = (float)1000000 / (float)TCFG_IIS_SAMPLE_RATE;
        iis_output_hdl->irq_cnt_tar = 4000;
        if (iis_output_hdl->syncts_en) {
            audio_iis_syncts_latch_trigger();
        }
        iis_output_hdl->state = 1;
        /* alink_start(); */
    }
}

void audio_iis_output_stop()
{
    printf("audio_iis_output_stop\n");
    if (iis_output_hdl) {
        alink_channel_close(iis_output_hdl->ch);
        audio_link_uninit();

        if (iis_output_hdl->sr_cal_timer) {
            sys_hi_timer_del(iis_output_hdl->sr_cal_timer);
        }

        if (iis_output_hdl->hw_src) {
            audio_hw_src_stop(iis_output_hdl->hw_src);
            audio_hw_src_close(iis_output_hdl->hw_src);
            free(iis_output_hdl->hw_src);
            iis_output_hdl->hw_src = NULL;
        }

        iis_output_hdl->state = 0;
        free(iis_output_hdl);
        iis_output_hdl = NULL;
        clk_set_en(1);
    }
}



void audio_iis_count_to_syncts(int frames)
{
    /* printf("frames %d\n", frames); */
    struct audio_dac_sync_node *node;
    if (iis_output_hdl) {
        list_for_each_entry(node, &iis_output_hdl->sync_list, entry) {
            sound_pcm_update_frame_num(node->hdl, frames);
        }
    }
}

void audio_iis_syncts_latch_trigger()
{
    struct audio_dac_sync_node *node;

    if (iis_output_hdl) {
        list_for_each_entry(node, &iis_output_hdl->sync_list, entry) {
            sound_pcm_syncts_latch_trigger(node->hdl);
        }
    }
}

void audio_iis_add_syncts_handle(void *syncts)
{
    struct audio_dac_sync_node *node = (struct audio_dac_sync_node *)zalloc(sizeof(struct audio_dac_sync_node));
    node->hdl = syncts;

    if (iis_output_hdl) {
        list_add(&node->entry, &iis_output_hdl->sync_list);
        sound_pcm_syncts_latch_trigger(syncts);
    }
}

void audio_iis_remove_syncts_handle(void *syncts)
{
    struct audio_dac_sync_node *node;

    if (iis_output_hdl) {
        list_for_each_entry(node, &iis_output_hdl->sync_list, entry) {
            if (node->hdl == syncts) {
                goto remove_node;
            }
        }
    }
    return;
remove_node:

    list_del(&node->entry);
    free(node);
}


#endif

