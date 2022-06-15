/*
 ****************************************************************************
 *							Audio DMS DUT
 *
 *
 ****************************************************************************
 */
#include "audio_dms_tool.h"
#include "online_db_deal.h"
#include "commproc_dms.h"
#include "system/includes.h"
#include "app_config.h"

#if 0
#define dms_spp_log	printf
#else
#define dms_spp_log(...)
#endif

#define DMS_BIG_ENDDIAN 		1		//大端数据输出
#define DMS_LITTLE_ENDDIAN		2		//小端数据输出
#define DMS_SPP_OUTPUT_WAY		DMS_LITTLE_ENDDIAN

#if TCFG_AUDIO_DMS_DUT_ENABLE
static dms_spp_t *dms_spp_hdl = NULL;
static int dms_app_online_parse(u8 *packet, u8 size, u8 *ext_data, u16 ext_size);
int dms_spp_rx_packet(u8 *dat, u8 len);

#if TCFG_AUDIO_DUAL_MIC_ENABLE
//双麦ENC DUT 事件处理
extern void audio_aec_output_sel(CVP_OUTPUT_ENUM sel, u8 agc);
int dms_spp_event_deal(u8 *dat)
{
    if (dms_spp_hdl) {
        switch (dat[3]) {
        case DMS_TEST_MASTER_MIC:
            audio_aec_output_sel(DMS_OUTPUT_SEL_MASTER, 0);
            dms_spp_log("CMD:DMS_TEST_MASTER_MIC\n");
            break;
        case DMS_TEST_SLAVE_MIC:
            audio_aec_output_sel(DMS_OUTPUT_SEL_SLAVE, 0);
            dms_spp_log("CMD:DMS_TEST_SLAVE_MIC\n");
            break;
        case DMS_TEST_OPEN_ALGORITHM:
            audio_aec_output_sel(DMS_OUTPUT_SEL_DEFAULT, 1);
            dms_spp_log("CMD:DMS_TEST_OPEN_ALGORITHM\n");
            break;
        case CVP_DUT_NS_OPEN:
            aec_dms_ioctl(CVP_NS_SWITCH, 1, NULL);
            dms_spp_log("CMD:DMS/CVP_DUT_NS_OPEN\n");
            break;
        case CVP_DUT_NS_CLOSE:
            aec_dms_ioctl(CVP_NS_SWITCH, 0, NULL);
            dms_spp_log("CMD:DMS/CVP_DUT_NS_CLOSE\n");
            break;
        default:
            dms_spp_log("CMD:UNKNOW CMD!!!\n");
            return DMS_TEST_ACK_FALI;
        }
    }
    return DMS_TEST_ACK_SUCCESS;
}
#else
//单麦SMS/DNS DUT 事件处理
extern void aec_toggle(u8 toggle);
int dms_spp_event_deal(u8 *dat)
{
    if (dms_spp_hdl) {
        switch (dat[3]) {
        case DMS_TEST_MASTER_MIC:
            aec_toggle(0);
            dms_spp_log("CMD:SMS/DNS_TEST_MASTER_MIC\n");
            break;
        case DMS_TEST_OPEN_ALGORITHM:
            aec_toggle(1);
            dms_spp_log("CMD:SMS/DNS_TEST_OPEN_ALGORITHM\n");
            break;
        case CVP_DUT_NS_OPEN:
            dms_spp_log("CMD:SMS/CVP_DUT_NS_OPEN\n");
            aec_ioctl(CVP_NS_SWITCH, 1, NULL);
            break;
        case CVP_DUT_NS_CLOSE:
            dms_spp_log("CMD:SMS/CVP_DUT_NS_CLOSE\n");
            aec_ioctl(CVP_NS_SWITCH, 0, NULL);
            break;
        default:
            dms_spp_log("CMD:UNKNOW CMD!!!\n");
            return DMS_TEST_ACK_FALI;
        }
    }
    return DMS_TEST_ACK_SUCCESS;
}
#endif/*TCFG_AUDIO_DMS_DUT_ENABLE*/


void dms_spp_init(void)
{
    dms_spp_log("tx dat");
    if (dms_spp_hdl == NULL) {
        dms_spp_hdl = zalloc(sizeof(dms_spp_t));
    }
    app_online_db_register_handle(DB_PKT_TYPE_DMS, dms_app_online_parse);
}

void dms_spp_unit(void)
{
    free(dms_spp_hdl);
    dms_spp_hdl = NULL;
}

int dms_spp_tx_packet(u8 command)
{
    if (dms_spp_hdl) {
        dms_spp_hdl->tx_buf.magic = DMS_TEST_SPP_MAGIC;
        dms_spp_hdl->tx_buf.dat[0] = 0;
        dms_spp_hdl->tx_buf.dat[1] = 0;
        dms_spp_hdl->tx_buf.dat[2] = 0;
        dms_spp_hdl->tx_buf.dat[3] = command;
        dms_spp_log("tx dat");
#if DMS_SPP_OUTPUT_WAY == DMS_LITTLE_ENDDIAN	//	小端格式
        dms_spp_hdl->tx_buf.crc = CRC16((&dms_spp_hdl->tx_buf.dat), DMS_SPP_PACK_NUM - 4);
        put_buf((u8 *)&dms_spp_hdl->tx_buf.magic, DMS_SPP_PACK_NUM);
        app_online_db_send(DB_PKT_TYPE_DMS, (u8 *)&dms_spp_hdl->tx_buf.magic, DMS_SPP_PACK_NUM);
#else
        u16 crc_temp;
        int i;
        u8 dat[DMS_SPP_PACK_NUM];
        u8 dat_temp;
        memcpy(dat, &(dms_spp_hdl->tx_buf), DMS_SPP_PACK_NUM);
        crc_temp = CRC16(dat + 4, 6);
        printf("crc0x%x,0x%x", crc_temp, dms_spp_hdl->tx_buf.crc);
        for (i = 0; i < 6; i += 2) {	//小端数据转大端
            dat_temp = dat[i];
            dat[i] = dat[i + 1];
            dat[i + 1] = dat_temp;
        }
        crc_temp = CRC16(dat + 4, DMS_SPP_PACK_NUM - 4);
        dat[2] = crc_temp >> 8;
        dat[3] = crc_temp & 0xff;
        put_buf(dat, DMS_SPP_PACK_NUM);
        app_online_db_send(DB_PKT_TYPE_DMS, dat, DMS_SPP_PACK_NUM);
#endif

    }
    return 0;
}

static int dms_app_online_parse(u8 *packet, u8 size, u8 *ext_data, u16 ext_size)
{
    if (dms_spp_hdl) {
        dms_spp_hdl->parse_seq  = ext_data[1];
        dms_spp_rx_packet(packet, size);
    }
    return 0;
}


int dms_spp_rx_packet(u8 *dat, u8 len)
{
    if (dms_spp_hdl) {
        u8 dat_temp;
        u16 crc = 0;
        int ret = 0;
        u8 dat_packet[DMS_SPP_PACK_NUM];
        if (len > DMS_SPP_PACK_NUM) {
            return 1;
        }
        dms_spp_log("rx dat,%d\n", DMS_SPP_PACK_NUM);
        put_buf(dat, len);
        memcpy(dat_packet, dat, len);
        crc = CRC16(dat + 4, len - 4);
#if DMS_SPP_OUTPUT_WAY == DMS_BIG_ENDDIAN	//	小端格式
        for (int i = 0; i < 6; i += 2) {	//小端数据转大端
            dat_temp = dat_packet[i];
            dat_packet[i] = dat_packet[i + 1];
            dat_packet[i + 1] = dat_temp;
        }
#endif
        dms_spp_log("rx dat_packet");
        memcpy(&(dms_spp_hdl->rx_buf), dat_packet, 4);
        if (dms_spp_hdl->rx_buf.magic == DMS_TEST_SPP_MAGIC) {
            dms_spp_log("crc %x,dms_spp_hdl->rx_buf.crc %x\n", crc, dms_spp_hdl->rx_buf.crc);
            if (dms_spp_hdl->rx_buf.crc == crc || dms_spp_hdl->rx_buf.crc == 0x1) {
                memcpy(&(dms_spp_hdl->rx_buf), dat_packet, len);
                ret = dms_spp_event_deal(dms_spp_hdl->rx_buf.dat);
                dms_spp_tx_packet(ret);		//反馈收到命令
                return 0;
            }
        }
    }
    return 1;
}

#endif /*TCFG_AUDIO_DMS_DUT_ENABLE*/
