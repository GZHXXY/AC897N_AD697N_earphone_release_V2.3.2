#ifndef __TME_MSG_L_H__
#define __TME_MSG_L_H__

#include "typedef.h"

typedef enum {
    TME_PRO_STATUS_SUCCESS             = 0,//成功
    TME_PRO_STATUS_UNKOWN_CMD          = 1,//非法命令字
    TME_PRO_STATUS_PARAM_OVER_LIMIT    = 2,//参数超长
    TME_PRO_STATUS_PARAM_UNDERSIZE     = 3,//参数太短
    TME_PRO_STATUS_FAIL               = 4,//命令执行出错
    TME_PRO_STATUS_TIMEOUT             = 5,//等待回复超时
    TME_PRO_STATUS_SEND_BUSY           = 6,//数据流传输已开始
    TME_PRO_STATUS_SEND_IDLE           = 7,//数据流传输未开始
    TME_PRO_STATUS_BLOCK_CRC_ERR       = 8,//数据块CRC校验失败
    TME_PRO_STATUS_ALL_DATA_CRC_ERR    = 9,//全部数据CRC校验失败
    TME_PRO_STATUS_LENGTH_ERR          = 10,//数据接收长度出错
    TME_PRO_STATUS_OPEN_SPEECH_FAIL    = 11,//无法打开录音通道
    TME_PRO_STATUS_SPEECH_INIT_FAIL    = 12,//语音服务初始化失败
    TME_PRO_STATUS_RANDOM_NUM_CHECK_ERR = 13,//随机数校验失败

    TME_PRO_STATU_OTA_BATTERY_LOW           = 61,//电池低
    TME_PRO_STATU_OTA_CONGIG_ERR            = 61,
    TME_PRO_STATU_OTA_CRC_SEGMEMT_ERR       = 63,
    TME_PRO_STATU_OTA_CRC_FILE_ERR          = 64,
    TME_PRO_STATU_OTA_RECEIVE_SIZE_ERR      = 65,
    TME_PRO_STATU_OTA_WRITE_FLASH_ERR       = 66,
    TME_PRO_STATU_OTA_FIRMWARE_SIZE_ERR     = 67,



} TME_PRO_STATUS;



typedef enum {

    TME_OPCODE_SPEECH_START      = 101,//设备通知APP：开始AI语音，常用于按键触发AI语音
    TME_OPCODE_APP_SPEECH_STOP   = 102,//APP设置设备：停止录音，常用于APP VAD
    TME_OPCODE_GET_PRIVATE_INFO  = 103,//APP查询设备：PID、蓝牙MAC地址
    TME_OPCODE_APP_SPEECH_START  = 104,//APP设置设备：开始录音
    TME_OPCODE_GET_AUDIO_FORMAT  = 105,//APP查询设备：支持录音格式
    TME_OPCODE_SPEECH_STOP       = 108,//设备通知APP：停止AI语音，常用于说完松开按键时

    TME_OPCODE_SPEECH_SIRI_OPEN  = 110,
    TME_OPCODE_SPEECH_SIRI_CLOSE = 111,



    TME_OPCODE_AUTHR_ACTIVE      = 120,//APP查询设备：握手鉴权
    TME_OPCODE_AUTHR_PASV        = 121,//APP查询设备：反向鉴权
    TME_OPCODE_AUTHR_FAIL        = 122,//APP设置设备：鉴权失败，即将断开连接
    TME_OPCODE_WRITE_COOKIE      = 123,//APP设置设备：写Cookie
    TME_OPCODE_AUTH_LAST         = 124,//最近一次鉴权成功后生成的随机数

    TME_OPCODE_ONE_KEY_FUNC      = 130,//一键直达

    TME_OPCODE_LIGHT_SETTING     = 140,//APP设置设备：灯光设置（需回复，设备会记住设置）
    TME_OPCODE_LIGHT_CTL_SETTING = 141,//APP设置设备：连续节奏控制（无需回复，设备不会记住）
    TME_OPCODE_LIGHT_ASK_SETTING = 142,//APP查询设备：当前设备灯光设置
    TME_OPCODE_LIGHT_ON          = 143,//APP设置设备：开灯
    TME_OPCODE_LIGHT_OFF         = 144,//APP设置设备：关灯
    TME_OPCODE_NOTICE_LIGHT_ON   = 145,//设备通知APP：设备手动开灯
    TME_OPCODE_NOTICE_LIGHT_OFF  = 146,//设备通知APP：设备手动关灯

    TME_OPCODE_SUPPORT_MODE_LIST = 161,//APP查询设备：支持的模式列表
    TME_OPCODE_CURRENT_MODE      = 162,//APP查询设备：当前模式
    TME_OPCODE_EN_HIDDEN_MODE   = 163,//APP设置设备：激活隐藏的模式

    TME_OPCODE_OTA_CANCLE        = 242,
    TME_OPCODE_OTA_BUF_CHECK     = 243,
    TME_OPCODE_OTA_GET_VERSION   = 244,//获取当前固件版本号
    TME_OPCODE_OTA_DATA          = 245,
    TME_OPCODE_OTA_SET_BT_CONFIG = 247,//更改固件蓝牙信息命令
    TME_OPCODE_OTA_GET_CONFIG    = 248,//OTA配置信息获取命令

    TME_OPCODE_DATA              = 255,

} TME_OPCODE;


//一键直达功能参数
#define ONE_KEY_FUNC_ADD_OR_CANCEL_FAVORITE          0    /*收藏/取消收藏歌曲*/
#define ONE_KEY_FUNC_PLAY_FAVORITE                   1    /*播放收藏（即 “我喜欢”歌单）*/
#define ONE_KEY_FUNC_PLAY_RECOMMEND                  2    /*播放个性化推荐歌曲*/
#define ONE_KEY_FUNC_START_MUSIC_RECOGNITION         3    /*启动听歌识曲*/
#define ONE_KEY_FUNC_ON_OR_OFF_APP_EQ                4    /*开/关音效*/
#define ONE_KEY_FUNC_PLAY_RUNNING_FM                 5    /*跑步电台*/


//TME模式码
#define TME_MODE_INTELLIGENCE_COLLECTION        0x00
#define TME_MODE_GAME_MODE                      0x01
#define TME_MODE_PERSONALIZED_RECOMMENDATION    0x02
#define TME_MODE_SPORT_MODE                     0x08

//触发位置
#define TME_TRIGGER_POSITION_LEFT    0x01
#define TME_TRIGGER_POSITION_RIGHT   0x02
#define TME_TRIGGER_POSITION_ALL     0x03

//触发方式
#define TME_TRIGGER_WAY_CLICK           0x01
#define TME_TRIGGER_WAY_DOUBLE_CLICK    0x02
#define TME_TRIGGER_WAY_THIPLE_CLICK    0x03
#define TME_TRIGGER_WAY_LONG            0x04
#define TME_TRIGGER_WAY_THIPLE_ELSE     0x05

#endif
