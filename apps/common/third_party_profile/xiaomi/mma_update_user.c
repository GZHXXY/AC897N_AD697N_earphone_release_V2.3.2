#include "app_config.h"
#include "bt_tws.h"
#include "btstack/avctp_user.h"

void  mma_update_status_hook(u8 st)
{
    if (st) {
#if(OTA_TWS_SAME_TIME_ENABLE == 0)
#if(TCFG_USER_TWS_ENABLE == 1)
        void tws_cancle_all_noconn();
        tws_cancle_all_noconn();
#endif   //TCFG_USER_TWS_ENABLE
        user_send_cmd_prepare(USER_CTRL_WRITE_CONN_DISABLE, 0, NULL);
        user_send_cmd_prepare(USER_CTRL_WRITE_SCAN_DISABLE, 0, NULL);
        if (get_bt_tws_connect_status()) {
            user_send_cmd_prepare(USER_CTRL_TWS_DISCONNECTION_HCI, 0, NULL);
        }
#endif  //OTA_TWS_SAME_TIME_ENABLE == 0
    } else {
#if(OTA_TWS_SAME_TIME_ENABLE == 0)  //单耳升级在这里直接reset
        cpu_reset();
#endif
    }
}

