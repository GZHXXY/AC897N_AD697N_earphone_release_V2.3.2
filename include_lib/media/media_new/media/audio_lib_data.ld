
	. = ALIGN(4);
	audio_sync_data_begin = .;
        *(.audio_sync_data)
	audio_sync_data_end = .;
	. = ALIGN(4);
#if AUDIO_EFFECTS_GAIN_AT_RAM
		*(.audio_gain_code)
		*(.audio_gain_const)
#endif/*AUDIO_EFFECTS_GAIN_AT_RAM*/
	. = ALIGN(4);
	
    *(.jlsp_data)
    *(.sbc_data)
    *(.msbc_data)
    *(.cvsd_data)
    *(.aac_data)
	*(.sbc_eng_data)
	*(.bt_audioplc_data)
    *(.lf_audioplc_data)
