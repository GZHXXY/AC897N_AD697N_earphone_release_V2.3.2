#include "app_config.h"

#if (TME_EN)

#include "system/includes.h"
#include "media/includes.h"
#include "key_event_deal.h"
#include "system/event.h"

#include "classic/tws_api.h"
#include "btstack/avctp_user.h"
#include "btstack/TME/TME_main.h"

#include "spp_user.h"
#include "tone_player.h"
#include "app_main.h"

#if TCFG_USER_TWS_ENABLE
#include "bt_tws.h"
#endif

extern const u8 tme_support_mode[TME_SUPPORT_MODE_NUM];
extern TME_MODE_INFO tme_mode_info[TME_SUPPORT_MODE_NUM];

extern int tme_spp_send_data_check(u16 len);
extern int tme_spp_send_data(u8 *data, u16 len);
extern int tme_ble_send_data(u8 *data, u16 len);
extern int ai_mic_rec_start(void);
extern int ai_mic_rec_close(void);
extern void bt_api_all_sniff_exit(void);


extern APP_VAR app_var;

struct JL_TME_VAR {
    /* ble_state_e  JL_ble_status; */
    void (*start_speech)(void);
    void (*stop_speech)(void);
    struct ble_server_operation_t *tme_ble;
    struct spp_operation_t *tme_spp;
    volatile u8 tme_run_flag: 1;
        volatile u8 JL_spp_status: 1;
        volatile u8 JL_ble_status: 1;
        volatile u8 JL_auth_status: 1;
        volatile u8 speech_status;
    };

    struct HEAD_PACKET {
    u8          OpCode;
    u8          sn;     //Sequence Number
    u16         length;//小端对齐
    u16         key_msg;
    u8          data[0];
};

struct JL_TME_VAR jl_ai_var = {
    .tme_run_flag = 0,
    .JL_ble_status = 0,
    .JL_spp_status = 0,
    .JL_auth_status = 0,
};

#define __this (&jl_ai_var)


static u8 randcheck[8];//用来检测的随机数

u16 TME_speech_data_send(u8 *buf, u16 len)
{
    TME_ERR send_err = TME_ERR_NONE;
    send_err = TME_DATA_send(TME_OPCODE_DATA, buf, len, 0);
    if (send_err == TME_ERR_NONE) {
        return 0;
    } else if (send_err == TME_ERR_SEND_NOT_READY) {
        return 1;
    } else {
        //printf("^^^^^^^^^^^^^^^^^^^ %d\n", send_err);
        return 1;
    }

    return 0;
}


bool JL_spp_fw_ready(void *priv)
{
    return (__this->JL_spp_status ? true : false);
}


static s32 JL_spp_send(void *priv, void *data, u16 len)
{
    int ret = 1;
    if (len < 128) {
        /* printf("send: \n"); */
        /* put_buf(data, (u32)len); */
    }
    if (__this->JL_spp_status) {
        if ((tme_spp_send_data_check(len)) && (__this->JL_spp_status == 1)) {
            ret = tme_spp_send_data(data, len);
            if (ret == 0) {
                return 0;
            } else if (ret == SPP_USER_ERR_SEND_BUFF_BUSY) {
                return 1;
            }
        }
    }

    return -1;
}

static u8 TME_wait_resp_timeout(void *priv, u8 OpCode, u8 *data, u16 length, u8 counter)
{
    printf("TME_wait_resp_timeout =%d \n", OpCode);
    return 0;
}


static void TME_msg_notif(u8 OpCode, u8 OpCode_SN, u8 *data, u16 len)
{
    struct sys_event e;
    struct HEAD_PACKET *head;
    e.type = SYS_BT_AI_EVENT;
    e.arg = "TME";
    head =  malloc(sizeof(struct HEAD_PACKET) + len); //可能要优化内存碎片问题
    if (!head) {
        printf("~~~TME_msg_notif malloc failed \n");
        return;
    }
    e.u.ai.value = (int)head;
    head->OpCode = OpCode;
    head->sn = OpCode_SN;
    head->length = len;
    if (len) {
        memcpy(head->data, data, len);
    }

    sys_event_notify(&e);
}

void TME_key_msg_notif(u16 msg, u8 *data, u16 len)
{
    struct sys_event e;
    struct HEAD_PACKET *head;
    e.type = SYS_BT_AI_EVENT;
    e.arg = "TME";
    head =  malloc(sizeof(struct HEAD_PACKET) + len); //可能要优化内存碎片问题
    if (!head) {
        printf("~~~TME_key_msg_notif malloc failed \n");
        return;
    }
    e.u.ai.value = (int)head;
    head->OpCode = 0;//OpCode;
    head->sn = 0;//OpCode_SN;
    head->key_msg = msg;
    head->length = len;
    if (len) {
        memcpy(head->data, data, len);
    }

    sys_event_notify(&e);
}


static void TME_CMD_callback(void *priv, u8 OpCode, u8 OpCode_SN, u8 *data, u16 len, u8 err_flag)
{
    printf("TME_CMD_callback OpCode = %d, sn =%x data_len = %d\n", OpCode, OpCode_SN, len);
    int msg_err = 0;//MSG_NO_ERROR;
    put_buf((u8 *)data, len);
    u32 time;

    switch (OpCode) {
    case TME_OPCODE_APP_SPEECH_START:
        TME_msg_notif(OpCode, OpCode_SN, data, len);
        /* msg_err = task_post_msg(NULL, 3, MSG_TME_SPEECH_APP_START,OpCode,OpCode_SN); */
        break;
    case TME_OPCODE_APP_SPEECH_STOP:
        TME_msg_notif(OpCode, OpCode_SN, data, len);
        /* void esco_enc_close(); */
        /* esco_enc_close(); */
        /* msg_err = task_post_msg(NULL, 3, MSG_TME_SPEECH_APP_STOP,OpCode,OpCode_SN); */
        break;
    case TME_OPCODE_SUPPORT_MODE_LIST:
        TME_msg_notif(OpCode, OpCode_SN, data, len);
        break;
    case TME_OPCODE_CURRENT_MODE:
        TME_msg_notif(OpCode, OpCode_SN, data, len);
        break;
    case TME_OPCODE_EN_HIDDEN_MODE:
        TME_msg_notif(OpCode, OpCode_SN, data, len);
        break;
    default:
        TME_msg_notif(OpCode, OpCode_SN, data, len);
        /* TME_CMD_response_send(OpCode,TME_PRO_STATUS_SUCCESS,OpCode_SN,data,len); */
        break;
    }


}

static void TME_CMD_no_resp_callback(void *priv, u8 OpCode, u8 *data, u16 len)
{
    /* printf("TME_CMD_no_resp_callback OpCode = %d, data_len = %d \n", OpCode, len); */
    /* put_buf((u8*)data, len); */
}



static void TME_DATA_no_resp_callback(void *priv, u8 OpCode_SN, u8 *data, u16 len)
{

    /* printf("TME_DATA_no_resp_callback OpCode_SN = %x, data_len = %d \n",OpCode_SN, len); */
    /* put_buf((u8*)data, len); */

}


static void TME_CMD_recieve_resp_callback(void *priv, u8 OpCode, u8 status, u8 *data, u16 len)
{
    int msg_err = 0;//MSG_NO_ERROR;
    printf("TME_CMD_recieve_resp_callback OpCode = %d  %x, data_len = %d \n", OpCode, status, len);
    /* printf_buf((u8*)data, len); */

    switch (OpCode) {
    case TME_OPCODE_SPEECH_SIRI_OPEN:
        if (status == TME_PRO_STATUS_SUCCESS) {
            /* TME_voice_command_speech_deal(); */
        }
        break;

    case TME_OPCODE_SPEECH_STOP:
        if (status == TME_PRO_STATUS_SUCCESS) {
            /* msg_err = task_post_msg(NULL, 1, MSG_TME_SPEECH_STOP); */
        } else {
            printf("TME_OPCODE_SPEECH_STOP fail ~\n");
        }
        break;

    case TME_OPCODE_SPEECH_START:
        if (status == TME_PRO_STATUS_SUCCESS) {
            printf(">>>>tme notify rec en \n");
            TME_msg_notif(OpCode, 0, NULL, 0);
        } else {
            printf("TME_OPCODE_SPEECH_START fail ~\n");
        }
        break;
    }
}



static const TME_PRO_CBK TME_Spp_callback = {
    .priv = (void *)NULL,
    .fw_ready = JL_spp_fw_ready,//通信是否准备好判断
    .fw_send = JL_spp_send,//发送接口
    .CMD_resp = TME_CMD_callback,//命令响应
    .DATA_resp = NULL,//TME_DATA_callback,//数据响应
    .CMD_no_resp = TME_CMD_no_resp_callback,
    .DATA_no_resp = TME_DATA_no_resp_callback,
    .CMD_recieve_resp = TME_CMD_recieve_resp_callback,
    .DATA_recieve_resp = NULL,//TME_DATA_recieve_resp_callback,
    .wait_resp_timeout = TME_wait_resp_timeout,
};

static bool JL_ble_fw_ready(void *priv)
{
    return (__this->JL_ble_status ? true : false);
}

static s32 JL_ble_send(void *priv, void *data, u16 len)
{
    int ret = 1;
    if (len < 128) {
        /* printf("send: \n"); */
        /* put_buf(data, (u32)len); */
    }

    if (__this->JL_ble_status == 1) {
        ret = tme_ble_send_data(data, len);
        if (ret == 0) {
            return 0;
        } else if (ret == SPP_USER_ERR_SEND_BUFF_BUSY) {
            return 1;
        }
    }

    return -1;
}

static const TME_PRO_CBK TME_BLE_callback = {
    .priv = (void *)NULL,
    .fw_ready = JL_ble_fw_ready,//通信是否准备好判断
    .fw_send = JL_ble_send,//发送接口
    .CMD_resp = TME_CMD_callback,//命令响应
    .DATA_resp = NULL,//TME_DATA_callback,//数据响应
    .CMD_no_resp = TME_CMD_no_resp_callback,
    .DATA_no_resp = TME_DATA_no_resp_callback,
    .CMD_recieve_resp = TME_CMD_recieve_resp_callback,
    .DATA_recieve_resp = NULL,//TME_DATA_recieve_resp_callback,
    .wait_resp_timeout = TME_wait_resp_timeout,
};


void ble_app_disconnect(void);
void bt_ble_adv_enable(u8 enable);

static u8 speech_busy = 0;

int TME_mic_is_running()
{
    return (!!speech_busy);
}


void TME_mic_start()
{
    if (!speech_busy) {
        ai_mic_rec_start();
        speech_busy = 1;
    }
}

void TME_mic_stop()
{
    if (speech_busy) {
        ai_mic_rec_close();
        speech_busy = 0;
    }
}

void phone_coming_ai_mic_close(void)
{
    printf(">>>>> tme phone coming ai mic close \n");
    int mic_coder_busy_flag(void);
    if (mic_coder_busy_flag()) {
        TME_mic_stop();
    }
}

#define SINK_TYPE_MASTER   1
u8 mic_get_data_source(void)
{
    return SINK_TYPE_MASTER;
}


int mic_coder_busy_flag(void)
{
    return TME_mic_is_running();
}

static u8 edr_connected = 0;

void TME_set_edr_connected(u8 flag)//连接过经典蓝牙
{
    edr_connected = flag;
}


u8 TME_get_edr_connected()//连接过经典蓝牙
{
    return edr_connected;
}

u8 get_TME_connect_type(void)
{
    if (__this->JL_spp_status) {
        return TME_SPP_SEL | (edr_connected >> 7);    //第七位为是否连接后经典蓝牙，断开不清0
    } else if (__this->JL_spp_status) {
        return TME_BLE_SEL | (edr_connected >> 7);
    } else {
        return TME_CON_NULL;
    }
}

bool TME_is_connected(void)
{
    return (__this->tme_run_flag != 0);
}

void TME_init(u8 connect_select)
{
    if (__this->tme_run_flag) {
        return;
    }

    memset((u8 *)__this, 0, sizeof(struct JL_TME_VAR));

    __this->start_speech = NULL;//start_speech;
    __this->stop_speech = NULL;//stop_speech;

    tme_mode_info_init();
    if (connect_select == TME_SPP_SEL) { //spp
        __this->JL_spp_status = 1;
        ble_app_disconnect();
        bt_ble_adv_enable(0);
        TME_protocol_init(&TME_Spp_callback,  0);
        /* printf("tme spp \n"); */
    } else if (connect_select == TME_BLE_SEL) { //ble
        __this->JL_ble_status = 1;
        TME_protocol_init(&TME_BLE_callback,  0);
        /* printf("tme ble \n"); */
    }
    __this->tme_run_flag = 1;
}



void TME_exit(u8 connect_select)
{
    if (!__this->tme_run_flag) {
        return;
    }

    if (connect_select == TME_SPP_SEL && !(__this->JL_spp_status)) { //spp
        return;
    } else if (connect_select == TME_BLE_SEL && !(__this->JL_ble_status)) { //ble
        return;
    }

    if (TME_mic_is_running()) {
        TME_mic_stop();
    }

    __this->JL_ble_status = 0;
    __this->JL_spp_status = 0;
    TME_protocol_exit();
    __this->JL_auth_status = 0;
    __this->tme_run_flag = 0;

#if TCFG_USER_TWS_ENABLE
    if ((tws_api_get_role() == TWS_ROLE_MASTER) && (connect_select == TME_SPP_SEL)) {
        bt_ble_adv_enable(1);
    }
#else
    if (connect_select == TME_SPP_SEL) {
        bt_ble_adv_enable(1);
    }
#endif


}


extern void tme_spp_disconnect(void *priv);

int TME_get_auth_status()
{
    return __this->JL_auth_status;
}


void TME_ota_start_callback(u8 start)//ota 结束 开始会调用该函数
{
    printf("TME_ota_start_callback\n");
#if TCFG_USER_TWS_ENABLE

#endif
}

void TME_auth_result(u8 result)
{
#if TCFG_USER_TWS_ENABLE
    if (!result && (tws_api_get_role() == TWS_ROLE_MASTER)) {
        printf("TME auth timeout \n");
        if (__this->JL_spp_status) {
            tme_spp_disconnect(NULL);
        } else if (__this->JL_ble_status) {
            ble_app_disconnect();
        }
    } else if (result && (tws_api_get_role() == TWS_ROLE_MASTER)) {
        __this->JL_auth_status = 1;
    }
#else
    if (!result) {
        printf("TME auth timeout \n");
        if (__this->JL_spp_status) {
            tme_spp_disconnect(NULL);
        } else if (__this->JL_ble_status) {
            ble_app_disconnect();
        }
    } else if (result) {
        __this->JL_auth_status = 1;
    }

#endif

}
static u32 clock_backup = 0;
void TME_auth_need_ctrl_clock(u8 status)
{
    //TME认证运算可能需要提高系统时钟。在这个函数操作。
    //1标识运算前，0标识运算之后恢复
    r_printf("need to set clock %d\n", status);
    if (status) {
        clock_backup = clk_get("sys");
        clk_set("sys", 128 * 1000000);
    } else {
        clk_set("sys", clock_backup);
    }
}
void phone_call_begin_ai(void)
{
    if (TME_mic_is_running()) {
        TME_mic_stop();
        TME_CMD_send(TME_OPCODE_SPEECH_STOP, NULL, 0, 1);
    }
}

void phone_call_end_ai(void)
{

}

//一键直达功能例子
void one_key_start_music_recognition(void)
{
    //发送一键直达的一个例子。可以copy一份改变data[0]赋值就行
#if 0
    int tme_err = 0;
    u8 data[2];
    data[0] = ONE_KEY_FUNC_START_MUSIC_RECOGNITION;
    tme_err = TME_CMD_send(TME_OPCODE_ONE_KEY_FUNC, data, 1, 1);
#else
    int tme_err = 0;
    u16 data;
    data = ONE_KEY_FUNC_START_MUSIC_RECOGNITION;
    tme_err = TME_CMD_send(TME_OPCODE_ONE_KEY_FUNC, &data, 2, 1);
#endif
}

int tme_hidden_mode_set(u8 mode, u8 res_word, u8 trigger_position, u8 trigger_way)
{
    u8 set_success = 0;
    for (int i = 0; i < TME_SUPPORT_MODE_NUM; i++) {
        if ((mode != tme_mode_info[i].mode) && (tme_mode_info[i].position == trigger_position) && (tme_mode_info[i].way == trigger_way)) {
            //新设置的按键位置和触发方式与已有模式重复，将重复的那个模式激活状态置零
            tme_mode_info[i].mode_state = 0;
        }
        if (mode == tme_mode_info[i].mode) {
            printf("set mode:%d, trig_position:%d, trig_way:%d", tme_support_mode[i], trigger_position, trigger_way);
            tme_mode_info[i].reserved_word = res_word;
            tme_mode_info[i].position = trigger_position;
            tme_mode_info[i].way = trigger_way;
            tme_mode_info[i].mode_state = 1;
            set_success = 1;
        }
    }
    if (set_success) {
        return TME_PRO_STATUS_SUCCESS;
    }
    return TME_PRO_STATUS_FAIL;
}

static u8 mic_timeout = 0;

static void TME_timeout_check(void *p)
{
    if (TME_mic_is_running()) {
        mic_timeout++;
        if (mic_timeout > 5) {
            TME_mic_stop();
            return;
        }
        bt_api_all_sniff_exit();
        sys_timeout_add(NULL, TME_timeout_check, 2000);
    }
}

void tme_event_post(u32 type, u8 event)
{
    struct sys_event e;
    e.type = SYS_BT_EVENT;
    e.arg  = (void *)type;
    e.u.bt.event = event;
    sys_event_notify(&e);
}


int TME_key_msg_deal(int msg)
{
    TME_ERR tme_err = 0;
    int ret = false;
    switch (msg) {
    case KEY_SEND_SPEECH_STOP:
        if (TME_mic_is_running()) {
            TME_mic_stop();
            TME_CMD_send(TME_OPCODE_SPEECH_STOP, NULL, 0, 1);
        }
        break;
    case KEY_SEND_SPEECH_START:
        printf("MSG_TME_SEND_SPEECH_START %d\n", clk_get("sys"));
        if (get_call_status() == BT_CALL_INCOMING) {//电话打入，挂断电话
            user_send_cmd_prepare(USER_CTRL_HFP_CALL_HANGUP, 0, NULL);
            break;
        }
        if ((get_call_status() == BT_CALL_ACTIVE)
            || (get_call_status() == BT_CALL_OUTGOING)
            || (get_call_status() == BT_CALL_ALERT)
           ) {
            break;
        }

        if (app_var.siri_stu == 2) {
            break;
        }

        if (TME_mic_is_running()) {
            TME_mic_stop();
//            TME_CMD_send(TME_OPCODE_SPEECH_STOP, NULL, 0, 1);
//            break;
        }

        tme_err = TME_CMD_send(TME_OPCODE_SPEECH_START, NULL, 0, 1);
        if ((tme_err != TME_ERR_NONE) && (tme_err != TME_ERR_AUTH)) {
#if TCFG_USER_TWS_ENABLE
            if (tws_api_get_role() == TWS_ROLE_MASTER) {
                /* user_send_cmd_prepare(USER_CTRL_HFP_GET_SIRI_OPEN, 0, NULL); */
            }
#else
            /* user_send_cmd_prepare(USER_CTRL_HFP_GET_SIRI_OPEN, 0, NULL); */
#endif
        }
        ret = true;
        break;
    default:

        ret = false;
        break;
    }

    return ret;
}


void TME_msg_deal(void *parm)
{
    u8 *buf;
    u8 buf_len;
    TME_ERR tme_err = 0;
    struct sys_event *e;
    struct HEAD_PACKET *head;
    if (!parm) {
        return;
    }
    e = (struct sys_event *)parm;
    head = (struct HEAD_PACKET *)e->u.ai.value;

    if (!head->OpCode && head->key_msg) {
        TME_key_msg_deal(head->key_msg);
        free(head);
        return;
    }

    switch (head->OpCode) {
    case TME_OPCODE_APP_SPEECH_START:
        printf("TME_OPCODE_APP_SPEECH_START\n");
        if (app_var.siri_stu == 2) {
            tme_err = TME_CMD_response_send((u8)head->OpCode, TME_PRO_STATUS_OPEN_SPEECH_FAIL, (u8)head->sn, NULL, 0);
            break;
        }
        tme_err = TME_CMD_response_send((u8)head->OpCode, TME_PRO_STATUS_SUCCESS, (u8)head->sn, NULL, 0);

    case TME_OPCODE_SPEECH_START:
        printf(">>TME_OPCODE_SPEECH_START \n");
        if (TME_mic_is_running()) {
            TME_mic_stop();
        }
        //tone_sin_play(250, 1);
        bt_api_all_sniff_exit();
        sys_timeout_add(NULL, TME_timeout_check, 2000);
        TME_mic_start();
        mic_timeout = 0;
        break;
    case TME_OPCODE_APP_SPEECH_STOP:
        printf("TME_OPCODE_APP_SPEECH_STOP\n");
        tme_err = TME_CMD_response_send((u8)head->OpCode, TME_PRO_STATUS_SUCCESS, (u8)head->sn, NULL, 0);
        TME_mic_stop();
        break;
    case TME_OPCODE_SUPPORT_MODE_LIST:
        printf("tme get support mode list\n");
        buf_len = (TME_SUPPORT_MODE_NUM + 1) * 2;
        buf = malloc(buf_len);
        memset(buf, 0, buf_len);
        buf[0] = 0;
        buf[1] = TME_SUPPORT_MODE_NUM;
        for (int i = 0; i < TME_SUPPORT_MODE_NUM; i++) {
            buf[2 + 2 * i] = tme_support_mode[i]; //mode_code
        }
        put_buf(buf, buf_len);
        TME_DATA_send((u8)head->OpCode, buf, buf_len, 0);
        free(buf);
        break;
    case TME_OPCODE_CURRENT_MODE:
        u8 active_mode_cnt = 0;
        buf = malloc((TME_SUPPORT_MODE_NUM * 4) + 2);
        memset(buf, 0, buf_len);
        buf[0] = 0;
        for (int i = 0; i < TME_SUPPORT_MODE_NUM; i++) {
            if (tme_mode_info[i].mode_state == 1) {
                buf[2 + 4 * active_mode_cnt] = tme_mode_info[i].mode;
                buf[2 + 4 * active_mode_cnt + 1] = tme_mode_info[i].reserved_word;
                buf[2 + 4 * active_mode_cnt + 2] = tme_mode_info[i].position;
                buf[2 + 4 * active_mode_cnt + 3] = tme_mode_info[i].way;
                active_mode_cnt += 1;
            }
        }
        buf[1] = active_mode_cnt;
        buf_len = (4 * active_mode_cnt) + 2;
        put_buf(buf, buf_len);
        TME_DATA_send((u8)head->OpCode, buf, buf_len, 0);
        free(buf);
        break;
    case TME_OPCODE_EN_HIDDEN_MODE:
        put_buf(head->data, head->length);
        u8 ret_state = 0;
        u8 mode_num = head->data[0];
        for (int i = 0; i < mode_num; i ++) {
            ret_state = tme_hidden_mode_set(head->data[i + 1], head->data[i + 2], head->data[i + 3], head->data[i + 4]);
            if (ret_state != 0) {
                TME_DATA_send((u8)head->OpCode, &ret_state, 1, 0);
                return;
            }
        }
        TME_DATA_send((u8)head->OpCode, &ret_state, 1, 0);
        break;
    default:
        break;
    }
    free(head);

}

#endif
