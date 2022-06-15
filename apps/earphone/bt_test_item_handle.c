
#include "app_config.h"

#define LOG_TAG             "[BT_TEST]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

void eartch_testbox_flag(u8 flag);

enum {
    TEST_STATE_INIT = 1,
    TEST_STATE_WAIT_REPORT,
    TEST_STATE_EXIT,
    TEST_STATE_TIMEOUT,
};

enum {
    ITEM_KEY_STATE_DETECT = 0,
    ITEM_IN_EAR_DETECT,
    ITEM_LP_TOUCH_KEY_SEN_DETECT,
};

static u8 in_ear_detect_test_flag = 0;
void testbox_in_ear_detect_test_flag_set(u8 flag)
{
    in_ear_detect_test_flag = flag;
}

u8 testbox_in_ear_detect_test_flag_get(void)
{
    return in_ear_detect_test_flag;
}

static void bt_in_ear_detection_test_state_handle(u8 state, u8 *value)
{
    switch (state) {
    case TEST_STATE_INIT:
        testbox_in_ear_detect_test_flag_set(1);

#if TCFG_EARTCH_EVENT_HANDLE_ENABLE
        eartch_testbox_flag(1);
#endif
        //start trim
        break;
    case TEST_STATE_EXIT:
        testbox_in_ear_detect_test_flag_set(0);
#if TCFG_EARTCH_EVENT_HANDLE_ENABLE
        eartch_testbox_flag(0);
#endif
        break;
    }
}
typedef struct lp_touch_key_test_cmd {
    u8 cmd;
} LP_TOUCH_TESTBOX_CMD;

struct lp_touch_key_test_report {
    u8 result;
    u16 res_max;
    u16 res_min;
    u16 res_delta;
    u16 res_percent;
    u8 fall_cnt;
    u8 raise_cnt;
};

enum LP_TOUCH_KEY_TESTBOX_CMD_TABLE {
    LP_TOUCH_KEY_TESTBOX_CMD_TESTMODE_NONE = 0,
    LP_TOUCH_KEY_TESTBOX_CMD_TESTMODE_ENTER = 'T',
    LP_TOUCH_KEY_TESTBOX_CMD_TEST_KEY,
    LP_TOUCH_KEY_TESTBOX_CMD_TEST_TIMEOUT_REPORT, //测试盒超时, 请求测试结果
    LP_TOUCH_KEY_TESTBOX_CMD_TESTMODE_EXIT,
};

extern int lp_touch_key_receive_cmd_from_testbox(void *priv);

static void bt_lp_touch_key_detection_test_state_handle(u8 state, u8 *value)
{
    LP_TOUCH_TESTBOX_CMD test_cmd;
    switch (state) {
    case TEST_STATE_INIT:
        test_cmd.cmd = LP_TOUCH_KEY_TESTBOX_CMD_TESTMODE_ENTER;
        break;
    case TEST_STATE_WAIT_REPORT:
        test_cmd.cmd = LP_TOUCH_KEY_TESTBOX_CMD_TEST_KEY;
        break;
    case TEST_STATE_EXIT:
        test_cmd.cmd = LP_TOUCH_KEY_TESTBOX_CMD_TESTMODE_EXIT;
        break;
    case TEST_STATE_TIMEOUT:
        test_cmd.cmd = LP_TOUCH_KEY_TESTBOX_CMD_TEST_TIMEOUT_REPORT;
        break;

    }

    lp_touch_key_receive_cmd_from_testbox(&test_cmd);
}

extern int bt_testbox_test_report_send_api(u8 item, void *info, u8 len);
int lp_touch_key_testbox_test_cmd_send(void *priv)
{
    bt_testbox_test_report_send_api(ITEM_LP_TOUCH_KEY_SEN_DETECT, priv, sizeof(struct lp_touch_key_test_report));
    return 0;
}

void bt_vendor_test_mode_event_handle(u8 *arg, u8 len)
{
    u8 test_item = arg[0];
    u8 state = arg[1];

    if (ITEM_IN_EAR_DETECT == test_item) {
        bt_in_ear_detection_test_state_handle(state, NULL);
    }
#if TCFG_LP_TOUCH_KEY_ENABLE
    else if (ITEM_LP_TOUCH_KEY_SEN_DETECT == test_item) {
        bt_lp_touch_key_detection_test_state_handle(state, NULL);
    }
#endif
}

