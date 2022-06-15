#include "app_config.h"
#include "key_event_deal.h"

#if (XM_MMA_EN)
#include "syscfg_id.h"
#include "bt_tws.h"

enum {
    MMA_INFO_SEND    = 0x1,
    MMA_AUTH_STATUS  = 0x2,
    MMA_SLAVE_REQ_AUTH_STATUS  = 0x3,
};

extern u32 tws_link_read_slot_clk();
extern void XM_protocol_update_auth_flag_tws(u8 flag);
extern void XM_protocol_send_auth_flag();
static void mma_tws_get_data_analysis(u8 cmd, u8 *data, u8 len)
{
    printf("get mma_tws_get_data_analysis %d %d\n", cmd, len);
    switch (cmd) {
    case MMA_AUTH_STATUS:
        r_printf("updata auth status %d-%d", data[1], data[2]);
        XM_protocol_update_auth_flag_tws(data[1]);
        break;
    case MMA_SLAVE_REQ_AUTH_STATUS:
        r_printf("get slave request\n");
        XM_protocol_send_auth_flag();
        break;
    }
}
static void MMA_tws_get_data_from_sibling(void *_data, u16 len, bool rx)
{
    u8 *data = _data;
    y_printf("rx-flag %d\n", rx);
    put_buf(_data, len);
    if (rx) {
        mma_tws_get_data_analysis(data[0], &data[1], len - 1);
    }
}
#define TWS_FUNC_ID_MMA_INFO_SYNC       TWS_FUNC_ID('M', 'M', 'A','P')
REGISTER_TWS_FUNC_STUB(mma_sync_info) = {
    .func_id = TWS_FUNC_ID_MMA_INFO_SYNC,
    .func    = MMA_tws_get_data_from_sibling,
};

void mma_tws_send_data(u8 cmd, u8 opcode, u8 *data, u8 len)
{
    int ret = 0;
#if TCFG_USER_TWS_ENABLE
    struct tws_sync_info_t send_data;
    memset((u8 *)&send_data, 0, sizeof(struct tws_sync_info_t));
    if (tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED) {
        if (tws_api_get_role() == TWS_ROLE_MASTER) {
            u32 master_clk = 0;
            switch (opcode) {
            case MMA_INFO_SEND:
                send_data.type = opcode;
                send_data.u.data[0] = opcode;
                send_data.u.data[1] = 0;
                master_clk = tws_link_read_slot_clk();
                send_data.u.data[2] = master_clk & 0xff;
                send_data.u.data[3] = (master_clk >> 8) & 0xff;
                send_data.u.data[4] = (master_clk >> 16) & 0xff;
                send_data.u.data[5] = (master_clk >> 24) & 0xff;
                printf("\n\n\n\nmasterclk %0x\n", master_clk);
                break;
            case MMA_AUTH_STATUS:
                send_data.type = opcode;
                memcpy(&send_data.u.data[0], data, len);
                break;
            }
            ret = tws_api_send_data_to_sibling((u8 *)&send_data, sizeof(struct tws_sync_info_t), TWS_FUNC_ID_MMA_INFO_SYNC);
        }
        if (tws_api_get_role() == TWS_ROLE_SLAVE) {
            if (opcode == MMA_SLAVE_REQ_AUTH_STATUS) {
                send_data.type = MMA_SLAVE_REQ_AUTH_STATUS;
                ret = tws_api_send_data_to_sibling((u8 *)&send_data, sizeof(struct tws_sync_info_t), TWS_FUNC_ID_MMA_INFO_SYNC);
            }
        }
        printf("-----------------ret %d  %d\n", tws_api_get_role(), ret);
    }
#endif
}


#endif
