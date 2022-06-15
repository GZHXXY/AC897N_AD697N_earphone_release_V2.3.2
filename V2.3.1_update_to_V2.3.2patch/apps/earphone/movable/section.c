#include "app_config.h"

1:
#if ((TCFG_AUDIO_CVP_NS_MODE==CVP_DNS_MODE) || (TCFG_CVP_DEVELOP_ENABLE == 0))
.tws.text.cache.L2.irq
.lmp.text.cache.L2.irq
.bt_classic.text.cache.L2.irq
#endif
.lmp.text.cache.L2.a2dp
.tws.text.cache.L2.media_sync
.profile.text.cache.L2.a2dp
.audio_decoder.text.cache.L2.run
.audio_decoder.text.cache.L2.file_read
.audio_decoder.text.cache.L2.frame_read
.aac_decoder.text.cache.L2.run
#if (TCFG_ESCO_DL_NS_ENABLE == 1)
//下行降噪打开，ram需要动态加载，省出ram给降噪用
.mixer.text.cache.L2.run
.audio_eq.text.cache.L2.eq
.eq_codec.text.cache.L2.hweq
.audio_dac.text.cache.L2.dac
.audio_syncts.text.cache.L2.runtime
.audio_sync.text.cache.L2.src
.audio_sync.text.cache.L2.hw
.sbc_decoder.text.cache.L2.run
.audio_sbc_hw.text.cache.L2.run
.bt_decode.text.cache.L2.decoder
.bt_decode.text.cache.L2.a2dp
.bt_decode.text.cache.L2.mixer
.bt_audio_timestamp.text.cache.L2.a2dp
#endif/*TCFG_ESCO_DL_NS_ENABLE*/

//通话代码
2:
.bt_decode.text.cache.L2.esco
.audio_cvp.text.cache.L2.plc
.agc_adv.text
.plc.text



