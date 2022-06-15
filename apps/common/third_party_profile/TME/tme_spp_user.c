
#include "app_config.h"
#include "app_action.h"

#include "system/includes.h"
#include "spp_user.h"
#include "string.h"
#include "circular_buf.h"
#include "bt_tws.h"


#if 0
extern void printf_buf(u8 *buf, u32 len);
#define log_info          printf
#define log_info_hexdump  printf_buf
#else
#define log_info(...)
#define log_info_hexdump(...)
#endif

/* #define DEBUG_ENABLE */
/* #include "debug_log.h" */
#if TME_EN


#include "btstack/TME/TME_main.h"
static struct spp_operation_t *spp_api = NULL;
static u8 spp_state;

int tme_spp_send_data(u8 *data, u16 len)
{
    if (spp_api) {
        log_info("tme spp_api_tx(%d) \n", len);
        log_info_hexdump(data, len);
        return spp_api->send_data(NULL, data, len);
    }
    return SPP_USER_ERR_SEND_FAIL;
}

int tme_spp_send_data_check(u16 len)
{
    if (spp_api) {
        if (spp_api->busy_state()) {
            return 0;
        }
    }
    return 1;
}

static void tme_spp_state_cbk(u8 state)
{
    spp_state = state;
    switch (state) {
    case SPP_USER_ST_CONNECT:
        printf("SPP_USER_ST_CONNECT ~~~\n");
        void TME_init(u8 connect_select);
        TME_init(TME_SPP_SEL);
        break;

    case SPP_USER_ST_DISCONN:
        printf("SPP_USER_ST_DISCONN ~~~\n");
        void TME_exit(u8 connect_select);
        TME_exit(TME_SPP_SEL);
        break;

    default:
        break;
    }

}

static void tme_spp_send_wakeup(void)
{
    putchar('W');
    void TME_protocol_resume(void);
    TME_protocol_resume();
}

static void tme_spp_recieve_cbk(void *priv, u8 *buf, u16 len)
{
    //printf("tme --- spp_api_rx(%d) \n", len);
    //put_buf(buf, len);

    //loop send data for test
    /* if (tme_spp_send_data_check(len)) { */
    /* tme_spp_send_data(buf, len); */
    /* } */
#if TCFG_USER_TWS_ENABLE
    if (tws_api_get_role() == TWS_ROLE_MASTER) {
        void TME_protocol_data_recieve(void *priv, void *buf, u16 len);
        TME_protocol_data_recieve(NULL, buf, len);
    }
#else
    void TME_protocol_data_recieve(void *priv, void *buf, u16 len);
    TME_protocol_data_recieve(NULL, buf, len);
#endif

}

void tme_spp_init(void)
{
    printf(">>>>>>>>tme_spp_init \n");
    spp_state = 0;
    spp_get_operation_table(&spp_api);
    spp_api->regist_recieve_cbk(0, tme_spp_recieve_cbk);
    spp_api->regist_state_cbk(0, tme_spp_state_cbk);
    spp_api->regist_wakeup_send(NULL, tme_spp_send_wakeup);
}


void tme_spp_disconnect(void *priv)
{
    if (spp_api) {
        spp_api->disconnect(priv);
    }
}

#endif

