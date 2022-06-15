#include "app_config.h"
#include "classic/tws_api.h"
#include "tone_player.h"
#include "btstack/avctp_user.h"
#include "system/includes.h"
#include "media/includes.h"
#include "tone_player.h"
#include "earphone.h"

#include "app_config.h"
#include "app_action.h"

#include "btstack/avctp_user.h"
#include "btstack/btstack_task.h"
#include "btstack/frame_queque.h"
#include "user_cfg.h"
#include "aec_user.h"
#include "classic/hci_lmp.h"
#include "bt_common.h"
#include "bt_ble.h"
#include "pbg_user.h"
#if TCFG_USER_TWS_ENABLE
#include "bt_tws.h"
#endif


#if XM_MMA_EN

//bool MMA_is_connected(void);
int MMA_tone_get_backup_clk(void);
int mic_coder_busy_flag(void);
bool bt_is_sniff_close(void);
void bt_sniff_ready_clean(void);
u16 sys_timeout_add(void *priv, void (*func)(void *priv), u32 msec);
extern void XM_speech_start_prepare(u8 flag/*long or short*/);
//void MMA_msg_deal(void *parm);

static void tone_play_end_callback(void *priv)
{
    printf("MMA tone end callback \n");
    if (MMA_tone_get_backup_clk()) {
        clk_set("sys", MMA_tone_get_backup_clk());
    }
#if (TCFG_USER_TWS_ENABLE)
    if (tws_api_get_role() == TWS_ROLE_SLAVE) {
        return;
    }
#endif//(TCFG_USER_TWS_ENABLE)

    XM_speech_start_prepare(0);
}

static int tone_backup_clk = 0;
int MMA_tone_get_backup_clk(void)
{
    return tone_backup_clk;
}

#if TCFG_USER_TWS_ENABLE
int MMA_tone_play_reset_clk(u8 index, int flag)
{
    int ret = tone_play_index_with_callback(index, 1, tone_play_end_callback, NULL);

    ///clear backup clock
    tone_backup_clk = 0;
    if (clk_get("sys") <= 24000000L) {
        tone_backup_clk = clk_get("sys");
        clk_set("sys", 48000000L);
    }
    return ret;

}

static int _MMA_tone_play(void)
{
    tone_play_index_with_callback(IDEX_TONE_NORMAL, 1, tone_play_end_callback, NULL);
    return 0;
}
static int MMA_tone_play(void)
{
    // 该函数是在中断里面调用，实际处理放在task里面
    int argv[8];
    argv[0] = (int)_MMA_tone_play;
    argv[1] = 0;
    int ret = os_taskq_post_type("app_core", Q_CALLBACK, 2, argv);
    /* printf("put taskq, ret:%d, len:%d \n", ret, len); */
    if (ret) {
        log_e("MMA taskq post err \n");
    }

    return ret;
}


static void tws_speech_tone_together(void *data, u16 len, bool rx)
{
    int err = 0;
    int *arg = (int *)data;
    int msg[8];
    msg[0] = (int)MMA_tone_play_reset_clk;
    msg[1] = 2;
    msg[2] = arg[0];
    msg[3] = arg[1];

    err = os_taskq_post_type("app_core", Q_CALLBACK, 2 + len / sizeof(int), msg);
    if (err) {
        log_e("tws speech tone togther error:%d \n", err);
    }
}

REGISTER_TWS_FUNC_STUB(tws_speech_tone) = {
    .func_id = 0xFFFF321A,
    .func    = tws_speech_tone_together,
};
#endif


static u16 MMA_tone_to_hdl = 0;
static u8 MMA_sniff_exit_cnt = 0;
static void MMA_start_tone_play(void *priv)
{
    MMA_tone_to_hdl = 0;
    if (!bt_is_sniff_close() || (MMA_sniff_exit_cnt == 0)) {
        MMA_sniff_exit_cnt = 0;
#if(TCFG_USER_TWS_ENABLE)
        if (get_tws_sibling_connect_state()) { //对耳已连接,需要同步提示音
            /*tws_api_sync_call_by_uuid('T', SYNC_CMD_START_SPEECH_TONE, 800);*/
            int data[2];
            data[0] = IDEX_TONE_NORMAL;
            data[1] = TONE_FLAG_KEY_START_UP_CLK;
            tws_api_send_data_to_slave(data, sizeof(data), 0xFFFF321A);

        } else
#endif
        {
            MMA_tone_play();
        }

    }

    if (MMA_sniff_exit_cnt) {
        printf("wait sniff exit:%d \n", MMA_sniff_exit_cnt);
        MMA_sniff_exit_cnt--;
        MMA_tone_to_hdl = sys_timeout_add(NULL, MMA_start_tone_play, 100);
    }
}

static int tone_play_after_bt_sniff_exit(void)
{
    if (MMA_tone_to_hdl) {
        sys_timeout_del(MMA_tone_to_hdl);
        MMA_tone_to_hdl = 0;
    }

    bt_sniff_ready_clean();

    if (!bt_is_sniff_close()) {
        MMA_sniff_exit_cnt = 0;
        MMA_tone_to_hdl = sys_timeout_add(NULL, MMA_start_tone_play, 10);
        return 0;
    }

    user_send_cmd_prepare(USER_CTRL_ALL_SNIFF_EXIT, 0, NULL);
    MMA_sniff_exit_cnt = 13;
    MMA_tone_to_hdl = sys_timeout_add(NULL, MMA_start_tone_play, 100);

    return (-1);
}

static int MMA_key_speech_start(void *arg)
{
    if (mic_coder_busy_flag()) {
        printf("ai mic busy\n");
        return (-1);
    }

    if (BT_STATUS_TAKEING_PHONE == get_bt_connect_status()) {
        printf("phone ing...\n");
        return (-1);
    }
    tone_backup_clk = 0;


    /*if ((!MMA_is_connected()) || (BT_STATUS_WAITINT_CONN == get_bt_connect_status())) {
        return -1;
    }*/


#if(TCFG_USER_TWS_ENABLE)
    if (tws_api_get_role() == TWS_ROLE_SLAVE) {
        return 0;
    }
#endif

    tone_play_after_bt_sniff_exit();
    return 0;
}

extern int ai_mic_rec_close(void);
static int bt_connction_status_event_handler(struct bt_event *bt)
{
    switch (bt->event) {
    case BT_STATUS_SCO_STATUS_CHANGE:
    case BT_STATUS_VOICE_RECOGNITION:
        printf(">>>>>>>>>>>>>>>>>>phone call close ai mic \n");
        ai_mic_rec_close();
        break;

    default:
        break;

    }

    return 0;
}
int a2dp_tws_dec_suspend(void *p);
void a2dp_tws_dec_resume(void);
static int bt_ai_event_handler(struct bt_event *bt)
{
    switch (bt->event) {
    case KEY_AI_DEC_SUSPEND:
        printf(">>>>>a2dp_suspend \n");
        a2dp_tws_dec_suspend(NULL);
        break;

    case KEY_AI_DEC_RESUME:
        printf(">>>>>a2dp_resume \n");
        a2dp_tws_dec_resume();
        break;

    }
    return 0;
}
/************************** KEY MSG****************************/
/*各个按键的消息设置，如果USER_CFG中设置了USE_CONFIG_KEY_SETTING为1，则会从配置文件读取对应的配置来填充改结构体*/
static const u8 key_table[KEY_NUM_MAX][KEY_EVENT_MAX] = {
    // SHORT           LONG              HOLD              UP              DOUBLE           TRIPLE
    {KEY_NULL,  	 KEY_NULL,  		KEY_NULL, 		 KEY_NULL,  	   KEY_SEND_SPEECH_START,  	   KEY_NULL},   //KEY_0
    {KEY_NULL,  	 KEY_NULL,  		KEY_NULL, 		 KEY_NULL,  	   KEY_NULL,  	   KEY_NULL},   //KEY_0
    {KEY_NULL,  	 KEY_NULL,  		KEY_NULL, 		 KEY_NULL,  	   KEY_NULL,  	   KEY_NULL},   //KEY_0
};
int mma_event_handler(struct sys_event *event)
{
    struct key_event *key;
    u8 key_event;

    switch (event->type) {
    case SYS_BT_AI_EVENT:
        //MMA_msg_deal((void *)event);
        return 1;

    case SYS_KEY_EVENT:
        key = &event->u.key;
        key_event = key_table[key->value][key->event];
        if (key_event == KEY_SEND_SPEECH_START) {
            printf(">>>>>>>>>>MMA key start ai<<<<<<<<<<<<<<<<<<<< \n");
            MMA_key_speech_start(event->arg);
            return 1;
        }
        break;
    case SYS_BT_EVENT:
        /*
         * 蓝牙事件处理
         */
        if ((u32)event->arg == SYS_BT_EVENT_TYPE_CON_STATUS) {
            bt_connction_status_event_handler(&event->u.bt);

        } else if (((u32)event->arg == SYS_BT_AI_EVENT_TYPE_STATUS)) {
            printf(">>>>>>>>>>>>>>>>>>bt ai event \n");
            bt_ai_event_handler(&event->u.bt);
        }

        break;

    default:
        break;
    }

    return (0);
}

#endif
