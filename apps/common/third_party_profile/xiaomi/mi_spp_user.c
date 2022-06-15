
#include "app_config.h"
#include "app_action.h"

#include "system/includes.h"
#include "spp_user.h"
#include "string.h"
#include "circular_buf.h"
#include "bt_common.h"
#include "3th_profile_api.h"
#include "app_config.h"

#if 1
extern void printf_buf(u8 *buf, u32 len);
#define log_info          printf
#define log_info_hexdump  printf_buf
#else
#define log_info(...)
#define log_info_hexdump(...)
#endif


#include "mi_spp_user.h"

#if XM_MMA_EN

static struct spp_operation_t *spp_api = NULL;
static u8 spp_state;

int mi_spp_send_data(u8 *data, u16 len)
{
    if (spp_api) {
        //log_info("spp_api_tx(%d) \n", len);
        /* log_info_hexdump(data, len); */
        return spp_api->send_data(NULL, data, len);
    }
    return SPP_USER_ERR_SEND_FAIL;
}

int mi_spp_send_data_check(u16 len)
{
    if (spp_api) {
        if (spp_api->busy_state()) {
            return 0;
        }
    }
    return 1;
}

static void mi_spp_state_cbk(u8 state)
{
    spp_state = state;
    switch (state) {
    case SPP_USER_ST_CONNECT:
        log_info("SPP_USER_ST_CONNECT ~~~\n");

        break;

    case SPP_USER_ST_DISCONN:
        log_info("SPP_USER_ST_DISCONN ~~~\n");
        break;

    default:
        break;
    }

}

static void mi_spp_send_wakeup(void)
{
    putchar('W');
}

static void mi_spp_recieve_cbk(void *priv, u8 *buf, u16 len)
{
    log_info("spp_api_rx(%d) \n", len);
    log_info_hexdump(buf, len);


    //loop send data for test
    if (mi_spp_send_data_check(len)) {
        mi_spp_send_data(buf, len);
    }
}



void mi_spp_disconnect(void)
{
    if (spp_api) {
        printf(">>>>>>>>>mi spp disconnect \n");
        spp_api->disconnect(NULL);
    }
}

void mi_spp_init(void)
{
    log_info("\n-%s-\n", __FUNCTION__);
    spp_state = 0;
    spp_get_operation_table(&spp_api);
    spp_api->regist_recieve_cbk(0, mi_spp_recieve_cbk);
    spp_api->regist_state_cbk(0, mi_spp_state_cbk);
    spp_api->regist_wakeup_send(NULL, mi_spp_send_wakeup);
}
#endif

