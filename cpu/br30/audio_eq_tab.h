#ifndef _AUDIO_EQ_TAB_H_
#define _AUDIO_EQ_TAB_H_

#include "system/includes.h"
#include "app_config.h"
#include "application/audio_eq.h"

#if (RCSP_ADV_EN)&&(JL_EARPHONE_APP_EN)&&(TCFG_DRC_ENABLE == 0)
#define AUDIO_EQ_Q          1.5f
#else
#define AUDIO_EQ_Q          0.7f
#endif

#if (TCFG_EQ_ENABLE == 1)

#if !TCFG_USE_EQ_FILE
static const struct eq_seg_info eq_tab_normal[EQ_SECTION_MAX_DEFAULT] = {
    {0, EQ_IIR_TYPE_BAND_PASS, 31,    0 << 20, (int)(AUDIO_EQ_Q * (1 << 24))},
    {1, EQ_IIR_TYPE_BAND_PASS, 62,    0 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {2, EQ_IIR_TYPE_BAND_PASS, 125,   0 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {3, EQ_IIR_TYPE_BAND_PASS, 250,   0 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {4, EQ_IIR_TYPE_BAND_PASS, 500,   0 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {5, EQ_IIR_TYPE_BAND_PASS, 1000,  0 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {6, EQ_IIR_TYPE_BAND_PASS, 2000,  0 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {7, EQ_IIR_TYPE_BAND_PASS, 4000,  0 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {8, EQ_IIR_TYPE_BAND_PASS, 8000,  0 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {9, EQ_IIR_TYPE_BAND_PASS, 16000, 0 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
};

static const struct eq_seg_info eq_tab_rock[EQ_SECTION_MAX_DEFAULT] = {
    {0, EQ_IIR_TYPE_BAND_PASS, 31,    -2 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {1, EQ_IIR_TYPE_BAND_PASS, 62,     0 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {2, EQ_IIR_TYPE_BAND_PASS, 125,    2 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {3, EQ_IIR_TYPE_BAND_PASS, 250,    4 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {4, EQ_IIR_TYPE_BAND_PASS, 500,   -2 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {5, EQ_IIR_TYPE_BAND_PASS, 1000,  -2 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {6, EQ_IIR_TYPE_BAND_PASS, 2000,   0 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {7, EQ_IIR_TYPE_BAND_PASS, 4000,   0 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {8, EQ_IIR_TYPE_BAND_PASS, 8000,   4 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {9, EQ_IIR_TYPE_BAND_PASS, 16000,  4 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
};

static const struct eq_seg_info eq_tab_pop[EQ_SECTION_MAX_DEFAULT] = {
    {0, EQ_IIR_TYPE_BAND_PASS, 31,     3 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {1, EQ_IIR_TYPE_BAND_PASS, 62,     1 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {2, EQ_IIR_TYPE_BAND_PASS, 125,    0 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {3, EQ_IIR_TYPE_BAND_PASS, 250,   -2 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {4, EQ_IIR_TYPE_BAND_PASS, 500,   -4 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {5, EQ_IIR_TYPE_BAND_PASS, 1000,  -4 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {6, EQ_IIR_TYPE_BAND_PASS, 2000,  -2 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {7, EQ_IIR_TYPE_BAND_PASS, 4000,   0 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {8, EQ_IIR_TYPE_BAND_PASS, 8000,   1 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {9, EQ_IIR_TYPE_BAND_PASS, 16000,  2 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
};

static const struct eq_seg_info eq_tab_classic[EQ_SECTION_MAX_DEFAULT] = {
    {0, EQ_IIR_TYPE_BAND_PASS, 31,     0 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {1, EQ_IIR_TYPE_BAND_PASS, 62,     8 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {2, EQ_IIR_TYPE_BAND_PASS, 125,    8 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {3, EQ_IIR_TYPE_BAND_PASS, 250,    4 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {4, EQ_IIR_TYPE_BAND_PASS, 500,    0 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {5, EQ_IIR_TYPE_BAND_PASS, 1000,   0 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {6, EQ_IIR_TYPE_BAND_PASS, 2000,   0 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {7, EQ_IIR_TYPE_BAND_PASS, 4000,   0 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {8, EQ_IIR_TYPE_BAND_PASS, 8000,   2 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {9, EQ_IIR_TYPE_BAND_PASS, 16000,  2 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
};

static const struct eq_seg_info eq_tab_country[EQ_SECTION_MAX_DEFAULT] = {
    {0, EQ_IIR_TYPE_BAND_PASS, 31,     -2 << 20, (int)(AUDIO_EQ_Q * (1 << 24))},
    {1, EQ_IIR_TYPE_BAND_PASS, 62,     0 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {2, EQ_IIR_TYPE_BAND_PASS, 125,    0 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {3, EQ_IIR_TYPE_BAND_PASS, 250,    2 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {4, EQ_IIR_TYPE_BAND_PASS, 500,    2 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {5, EQ_IIR_TYPE_BAND_PASS, 1000,   0 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {6, EQ_IIR_TYPE_BAND_PASS, 2000,   0 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {7, EQ_IIR_TYPE_BAND_PASS, 4000,   0 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {8, EQ_IIR_TYPE_BAND_PASS, 8000,   4 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {9, EQ_IIR_TYPE_BAND_PASS, 16000,  4 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
};

static const struct eq_seg_info eq_tab_jazz[EQ_SECTION_MAX_DEFAULT] = {
    {0, EQ_IIR_TYPE_BAND_PASS, 31,     0 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {1, EQ_IIR_TYPE_BAND_PASS, 62,     0 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {2, EQ_IIR_TYPE_BAND_PASS, 125,    0 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {3, EQ_IIR_TYPE_BAND_PASS, 250,    4 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {4, EQ_IIR_TYPE_BAND_PASS, 500,    4 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {5, EQ_IIR_TYPE_BAND_PASS, 1000,   4 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {6, EQ_IIR_TYPE_BAND_PASS, 2000,   0 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {7, EQ_IIR_TYPE_BAND_PASS, 4000,   2 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {8, EQ_IIR_TYPE_BAND_PASS, 8000,   3 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {9, EQ_IIR_TYPE_BAND_PASS, 16000,  4 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
};


static struct eq_seg_info eq_tab_custom[EQ_SECTION_MAX_DEFAULT] = {
    {0, EQ_IIR_TYPE_BAND_PASS, 31,    0 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {1, EQ_IIR_TYPE_BAND_PASS, 62,    0 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {2, EQ_IIR_TYPE_BAND_PASS, 125,   0 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {3, EQ_IIR_TYPE_BAND_PASS, 250,   0 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {4, EQ_IIR_TYPE_BAND_PASS, 500,   0 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {5, EQ_IIR_TYPE_BAND_PASS, 1000,  0 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {6, EQ_IIR_TYPE_BAND_PASS, 2000,  0 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {7, EQ_IIR_TYPE_BAND_PASS, 4000,  0 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {8, EQ_IIR_TYPE_BAND_PASS, 8000,  0 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},
    {9, EQ_IIR_TYPE_BAND_PASS, 16000, 0 << 20, (int)(AUDIO_EQ_Q  * (1 << 24))},

};

#endif
#endif /* (TCFG_EQ_ENABLE == 1) */

#endif
