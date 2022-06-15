
#include "board_config.h"
#include "asm/dac.h"
#include "media/includes.h"
#include "effectrs_sync.h"
#include "asm/gpio.h"


extern struct audio_dac_hdl dac_hdl;
extern int bt_media_sync_master(u8 type);
extern u8 bt_media_device_online(u8 dev);
extern void *bt_media_sync_open(void);
extern void bt_media_sync_close(void *);
extern int a2dp_media_get_total_buffer_size();
extern int bt_send_audio_sync_data(void *, void *buf, u32 len);
extern void bt_media_sync_set_handler(void *, void *priv,
                                      void (*event_handler)(void *, int *, int));
extern int bt_audio_sync_nettime_select(u8 basetime);


const static struct audio_tws_conn_ops tws_conn_ops = {
    .open = bt_media_sync_open,
    .set_handler = bt_media_sync_set_handler,
    .close = bt_media_sync_close,
    .master = bt_media_sync_master,
    .online = bt_media_device_online,
    .send = bt_send_audio_sync_data,
};

void *a2dp_play_sync_open(u8 channel, u32 sample_rate, u32 output_rate, u32 coding_type)
{
    struct audio_wireless_sync_info sync_param;

    sync_param.channel = channel;
    sync_param.tws_ops = &tws_conn_ops;
    sync_param.sample_rate = sample_rate;
    sync_param.output_rate = output_rate;

    sync_param.protocol = WL_PROTOCOL_RTP;
    sync_param.reset_enable = 0;//内部支持可复位

    sync_param.dev = &dac_hdl;

    bt_audio_sync_nettime_select(0);

    return audio_wireless_sync_open(&sync_param);
}

void *esco_play_sync_open(u8 channel, u16 sample_rate, u16 output_rate)

{
    struct audio_wireless_sync_info sync_param;

    int esco_buffer_size = 60 * 50;

    sync_param.channel = channel;
    sync_param.sample_rate = sample_rate;
    sync_param.output_rate = output_rate;
    sync_param.tws_ops = &tws_conn_ops;
    sync_param.reset_enable = 0;
    sync_param.dev = &dac_hdl;

    sync_param.protocol = WL_PROTOCOL_SCO;

    bt_audio_sync_nettime_select(0);

    return audio_wireless_sync_open(&sync_param);
}

void *local_file_dec_sync_open(u8 channel, u16 sample_rate, u16 output_rate)
{
    struct audio_wireless_sync_info sync_info = {0};

    sync_info.channel = channel;
    sync_info.sample_rate = sample_rate;
    sync_info.output_rate = output_rate;
    sync_info.tws_ops = &tws_conn_ops;
    sync_info.dev = &dac_hdl;
    sync_info.protocol = WL_PROTOCOL_FILE;
    sync_info.reset_enable = 0;

    bt_audio_sync_nettime_select(1);

    return audio_wireless_sync_open(&sync_info);
}

void *local_tws_dec_sync_open(u8 channel, u16 sample_rate, u16 output_rate)
{
    struct audio_wireless_sync_info sync_info = {0};

    sync_info.channel = channel;
    sync_info.sample_rate = sample_rate;
    sync_info.output_rate = output_rate;
    sync_info.tws_ops = &tws_conn_ops;
    sync_info.dev = &dac_hdl;
    sync_info.protocol = WL_PROTOCOL_JL_TWS;
    sync_info.reset_enable = 0;

    bt_audio_sync_nettime_select(1);

    return audio_wireless_sync_open(&sync_info);
}
