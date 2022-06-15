#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

/*
 *  板级配置选择
 */

//#define CONFIG_BOARD_FPGA
//#define CONFIG_BOARD_AC8976D_TWS
//#define CONFIG_BOARD_AC8972A_DEMO
//#define CONFIG_BOARD_AC8972A_T8
//#define CONFIG_BOARD_AC8976A_DMS	//双mic降噪(ENC)
//#define CONFIG_BOARD_AC8973A_ANC
//#define CONFIG_BOARD_AC8976A_ANC
//#define CONFIG_BOARD_AC8976A_NPU
//#define CONFIG_BOARD_AC897N_SD_DEMO
#define CONFIG_BOARD_AD697N_DEMO
// #define CONFIG_BOARD_AD697N_ANC
//#define CONFIG_BOARD_AD697N_T8
//#define CONFIG_BOARD_AD6973A_ANC
//#define CONFIG_BOARD_AC8976_MS70
//#define CONFIG_BOARD_AC699N_DEMO



#include "board_ac8976d_tws_cfg.h"
#include "board_ac8972a_demo_cfg.h"
#include "board_ac8976a_dms_cfg.h"
#include "board_ac8973a_anc_cfg.h"
#include "board_ac8976a_anc_cfg.h"
#include "board_ac8976a_npu_cfg.h"
#include "board_ac897n_sd_demo_cfg.h"
#include "board_ad697n_demo_cfg.h"
#include "board_ad697n_anc_cfg.h"

#include "board_ad6973a_anc_cfg.h"



#define  DUT_AUDIO_DAC_LDO_VOLT   				DACVDD_LDO_1_25V

#ifndef CONFIG_CHIP_NAME
#error "undefine CONFIG_CHIP_NAME"
#endif

#endif
