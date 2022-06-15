#include "app_config.h"

#include "btstack/TME/TME_main.h"
#include "key_event_deal.h"

#if (TME_EN)
#include "syscfg_id.h"
#include "bt_tws.h"

enum {
    TME_INFO_SEND = 0x1,
};

extern u32 tws_link_read_slot_clk();
void TME_tws_get_data_analysis(u8 cmd, u8 *data, u8 len);
static void TME_tws_get_data_from_sibling(void *_data, u16 len, bool rx);

#define TWS_FUNC_ID_TME_INFO_SYNC       TWS_FUNC_ID('T', 'M', 'E', 'I')
REGISTER_TWS_FUNC_STUB(tme_sync_info) = {
    .func_id = TWS_FUNC_ID_TME_INFO_SYNC,
    .func    = TME_tws_get_data_from_sibling,
};

static void TME_tws_get_data_from_sibling(void *_data, u16 len, bool rx)
{
    u8 *data = _data;
    TME_tws_get_data_analysis(data[0], &data[1], len - 1);
}

void TME_tws_send_data(u8 cmd, u8 opcode, u8 *data, u8 len)
{
#if TCFG_USER_TWS_ENABLE
    if (tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED) {
        if (tws_api_get_role() == TWS_ROLE_MASTER) {
            struct tws_sync_info_t send_data;
            u32 master_clk = 0;
            switch (opcode) {
            case TME_INFO_SEND:
                send_data.type = cmd;
                send_data.u.data[0] = opcode;
                send_data.u.data[1] = get_TME_connect_type();
                master_clk = tws_link_read_slot_clk();
                send_data.u.data[2] = master_clk & 0xff;
                send_data.u.data[3] = (master_clk >> 8) & 0xff;
                send_data.u.data[4] = (master_clk >> 16) & 0xff;
                send_data.u.data[5] = (master_clk >> 24) & 0xff;
                printf("\n\n\n\nmasterclk %0x\n", master_clk);
                break;
            }
            tws_api_send_data_to_sibling((u8 *)&send_data, sizeof(struct tws_sync_info_t), TWS_FUNC_ID_TME_INFO_SYNC);
        }
    }
#endif
}


void TME_tws_get_data_analysis(u8 cmd, u8 *data, u8 len)
{
    /* switch (cmd) { */
    /* case TWS_TME_MSG: */
    /*     switch (data[0]) { */
    /*         case TME_INFO_SEND: */
    /*             TME_set_edr_connected(data[1]&(TME_EDR_MASK)); */
    /*             break; */
    /*     } */
    /*     break; */
    /* } */
}


#endif
