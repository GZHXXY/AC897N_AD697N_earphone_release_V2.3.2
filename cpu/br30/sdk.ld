

















  local_irq_enable = ABSOLUTE(0x10fdf8);
  local_irq_disable = ABSOLUTE(0x10fdfc);
  lp_enter = ABSOLUTE(0x10fe00);
  lp_signature_set = ABSOLUTE(0x10fe04);
  os_init = ABSOLUTE(0x10fe08);
  os_start = ABSOLUTE(0x10fe0c);
  os_get_curr_tcb_var = ABSOLUTE(0x10fe10);
  os_task_create = ABSOLUTE(0x10fe14);
  os_current_task_rom = ABSOLUTE(0x10fe18);
  os_time_dly = ABSOLUTE(0x10fe1c);
  os_time_get = ABSOLUTE(0x10fe20);
  os_task_del_req = ABSOLUTE(0x10fe24);
  os_task_del_res = ABSOLUTE(0x10fe28);
  os_task_del = ABSOLUTE(0x10fe2c);
  __os_taskq_pend = ABSOLUTE(0x10fe30);
  os_taskq_pend = ABSOLUTE(0x10fe34);
  os_task_pend_rom = ABSOLUTE(0x10fe38);
  __os_taskq_post_rom = ABSOLUTE(0x10fe3c);


  os_taskq_flush_rom = ABSOLUTE(0x10fe48);



  os_taskq_accept = ABSOLUTE(0x10fe58);
  os_taskq_del_rom = ABSOLUTE(0x10fe5c);
  os_taskq_del_type = ABSOLUTE(0x10fe60);
  os_sem_create = ABSOLUTE(0x10fe64);
  os_sem_del = ABSOLUTE(0x10fe68);
  os_sem_pend_rom = ABSOLUTE(0x10fe6c);
  os_sem_post = ABSOLUTE(0x10fe70);
  os_sem_set = ABSOLUTE(0x10fe74);
  os_sem_valid = ABSOLUTE(0x10fe78);
  os_sem_query = ABSOLUTE(0x10fe7c);
  os_mutex_create = ABSOLUTE(0x10fe80);
  os_mutex_del = ABSOLUTE(0x10fe84);
  os_mutex_pend_rom = ABSOLUTE(0x10fe88);
  os_mutex_post = ABSOLUTE(0x10fe8c);
  os_mutex_valid = ABSOLUTE(0x10fe90);
  xTaskCreate = ABSOLUTE(0x10fe94);
  xTaskGetCurrentTaskHandle = ABSOLUTE(0x10fe98);
  xTaskGetTickCount = ABSOLUTE(0x10fe9c);
  xTaskGetHandle = ABSOLUTE(0x10fea0);
  vTaskDelete = ABSOLUTE(0x10fea4);
  xTaskGetApplicationTaskTag = ABSOLUTE(0x10fea8);
  vTaskSetApplicationTaskTag = ABSOLUTE(0x10feac);
  xQueueGenericCreateStatic = ABSOLUTE(0x10feb0);
  xQueueGenericReset = ABSOLUTE(0x10feb4);
  xQueueGenericSend = ABSOLUTE(0x10feb8);
  xQueueGenericSendFromISR = ABSOLUTE(0x10febc);
  xQueueGenericReceive = ABSOLUTE(0x10fec0);
  xQueueReceiveFromISR = ABSOLUTE(0x10fec4);
  vQueueDelete = ABSOLUTE(0x10fec8);
  uxQueueMessagesWaiting = ABSOLUTE(0x10fecc);
  uxQueueMessagesWaitingFromISR = ABSOLUTE(0x10fed0);
  uxQueueSpacesAvailable = ABSOLUTE(0x10fed4);
  pcTaskGetName = ABSOLUTE(0x10fed8);
  prvGetExpectedIdleTime = ABSOLUTE(0x10fedc);
  eTaskConfirmSleepModeStatus = ABSOLUTE(0x10fee0);
  vTaskStepTick = ABSOLUTE(0x10fee4);
  pxPortInitialiseStack = ABSOLUTE(0x10fee8);
  xPortStartScheduler = ABSOLUTE(0x10feec);
  vPortEndScheduler = ABSOLUTE(0x10fef0);
  vPortYield = ABSOLUTE(0x10fef4);
  vPortExitCritical = ABSOLUTE(0x10fef8);
  OS_ClrPending = ABSOLUTE(0x10fefc);
  vTickISR = ABSOLUTE(0x10ff00);
  pmalloc_init = ABSOLUTE(0x10ff04);
  pmalloc = ABSOLUTE(0x10ff08);
  pfree = ABSOLUTE(0x10ff0c);
  pmalloc_get_unused_pages = ABSOLUTE(0x10ff10);
  get_physic_address = ABSOLUTE(0x10ff14);
  pmm_get_var = ABSOLUTE(0x10ff18);
  vmm_init_ = ABSOLUTE(0x10ff1c);
  vmalloc_ = ABSOLUTE(0x10ff20);
  vfree_ = ABSOLUTE(0x10ff24);
  vmem_get_phy_adr = ABSOLUTE(0x10ff28);
  vmem_start = ABSOLUTE(0x10ff2c);
  vmem_end = ABSOLUTE(0x10ff30);
  vmm_get_var = ABSOLUTE(0x10ff34);
  vmalloc_v2 = ABSOLUTE(0x10ff38);
  vfree_v2 = ABSOLUTE(0x10ff3c);
  vcopy_ = ABSOLUTE(0x10ff40);
  get_cvsd_codec_ops = ABSOLUTE(0x10ff44);
  B_fir_cal_s = ABSOLUTE(0x10ff48);
  B_array_vol = ABSOLUTE(0x10ff4c);
  B_Syn_filt = ABSOLUTE(0x10ff50);
  B_Residu = ABSOLUTE(0x10ff54);
  B_comput_correlataionS = ABSOLUTE(0x10ff58);
  B_iircal = ABSOLUTE(0x10ff5c);
  memmem = ABSOLUTE(0x10ff60);
  memcpy = ABSOLUTE(0x10ff64);
  memmove = ABSOLUTE(0x10ff68);
  memcmp = ABSOLUTE(0x10ff6c);
  memset = ABSOLUTE(0x10ff70);
  strcmp = ABSOLUTE(0x10ff74);
  strcpy = ABSOLUTE(0x10ff78);
  strlen = ABSOLUTE(0x10ff7c);
  strncmp = ABSOLUTE(0x10ff80);
  strstr = ABSOLUTE(0x10ff84);
  strchr = ABSOLUTE(0x10ff88);
  IcuCmdKst = ABSOLUTE(0x10ff8c);
  DcuCmdKst = ABSOLUTE(0x10ff90);
  IcuCmdSend = ABSOLUTE(0x10ff94);
  DcuCmdSend = ABSOLUTE(0x10ff98);
  flush_dcache = ABSOLUTE(0x10ff9c);
  flushinv_dcache = ABSOLUTE(0x10ffa0);
  sfc_suspend = ABSOLUTE(0x10ffa4);
  sfc_resume = ABSOLUTE(0x10ffa8);
  sfc_drop_cache = ABSOLUTE(0x10ffac);
  chip_crc16 = ABSOLUTE(0x10ffb0);
  CrcDecode = ABSOLUTE(0x10ffb4);
  mask_init = ABSOLUTE(0x10ffb8);
  wdt_clr = ABSOLUTE(0x10ffbc);
  nvram_set_boot_state = ABSOLUTE(0x10ffc0);
  chip_reset = ABSOLUTE(0x10ffc4);
  the_debug_isr = ABSOLUTE(0x10ffc8);

  print = ABSOLUTE(0x10ffd0);
  sprintf = ABSOLUTE(0x10ffd4);
  printf = ABSOLUTE(0x10ffd8);
  puts = ABSOLUTE(0x10ffdc);
  doe = ABSOLUTE(0x10ffe0);
  spi_get_port = ABSOLUTE(0x10ffe4);
  _pll_init = ABSOLUTE(0x10ffe8);
  flash_poweron = ABSOLUTE(0x10ffec);
  LZ4_setStreamDecode = ABSOLUTE(0x10fff0);
  LZ4_decompress_safe_continue = ABSOLUTE(0x10fff4);
  LZ4_decompress_generic = ABSOLUTE(0x10fff8);
  LZ4_decompress_safe_usingDict = ABSOLUTE(0x10fffc);
  mdct_tab_256 = ABSOLUTE(0x1081ec);
  mdct_tab_2048 = ABSOLUTE(0x1083ec);
  sine_long_1024 = ABSOLUTE(0x1093ec);
  sine_short_128 = ABSOLUTE(0x10a3ec);
  kbd_long_1024 = ABSOLUTE(0x10a5ec);
  kbd_short_128 = ABSOLUTE(0x10b5ec);
  Huffmancb_SCL = ABSOLUTE(0x10b7ec);
  num_swb_960_window = ABSOLUTE(0x10b9fc);
  num_swb_1024_window = ABSOLUTE(0x10ba08);
  num_swb_128_window = ABSOLUTE(0x10ba14);
  swb_offset_1024_96 = ABSOLUTE(0x10ba20);
  swb_offset_128_96 = ABSOLUTE(0x10ba74);
  swb_offset_1024_64 = ABSOLUTE(0x10ba8e);
  swb_offset_128_64 = ABSOLUTE(0x10baee);
  swb_offset_1024_48 = ABSOLUTE(0x10bb08);
  swb_offset_128_48 = ABSOLUTE(0x10bb6c);
  swb_offset_1024_32 = ABSOLUTE(0x10bb8a);
  swb_offset_1024_24 = ABSOLUTE(0x10bbf2);
  swb_offset_128_24 = ABSOLUTE(0x10bc52);
  swb_offset_1024_16 = ABSOLUTE(0x10bc72);
  swb_offset_128_16 = ABSOLUTE(0x10bcca);
  swb_offset_1024_8 = ABSOLUTE(0x10bcea);
  swb_offset_128_8 = ABSOLUTE(0x10bd3c);
  swb_offset_1024_window = ABSOLUTE(0x10bd5c);
  swb_offset_128_window = ABSOLUTE(0x10bd8c);
  iq_table = ABSOLUTE(0x10bdbc);
  tns_coef_0_3 = ABSOLUTE(0x10c1c0);
  tns_coef_0_4 = ABSOLUTE(0x10c200);
  tns_coef_1_3 = ABSOLUTE(0x10c240);
  tns_coef_1_4 = ABSOLUTE(0x10c280);
  pow05_table = ABSOLUTE(0x10c2c0);
  sample_rates = ABSOLUTE(0x10c2dc);
  pred_sfb_max = ABSOLUTE(0x10c30c);
  tns_sbf_max = ABSOLUTE(0x10c318);
  Huffmancb_1 = ABSOLUTE(0x10c358);
  Huffmancb_2 = ABSOLUTE(0x10c4f0);
  Huffmancb_3 = ABSOLUTE(0x10c628);
  Huffmancb_4 = ABSOLUTE(0x10c760);
  Huffmancb_5 = ABSOLUTE(0x10c890);
  Huffmancb_6 = ABSOLUTE(0x10c9d8);
  Huffmancb_7 = ABSOLUTE(0x10cb18);
  Huffmancb_8 = ABSOLUTE(0x10cc10);
  Huffmancb_9 = ABSOLUTE(0x10cd08);
  Huffmancb_10 = ABSOLUTE(0x10cfa8);
  Huffmancb_11 = ABSOLUTE(0x10d238);
  AACcodeBookDTable = ABSOLUTE(0x10d6f8);
  AAC_SFHUFF_TAB = ABSOLUTE(0x10b9f4);
  STFT_Win_FixHalf_M512_D160 = ABSOLUTE(0x1040a0);
  STFT_Win_FixHalf_M256_D160 = ABSOLUTE(0x1044a0);
  STFT_Win_FixHalf_M256_D80 = ABSOLUTE(0x1046a0);
  STFT_Win_FixHalf_M128_D80 = ABSOLUTE(0x1048a0);
  WOLAAnalysisFilter_Float_M64_m4_r1 = ABSOLUTE(0x1049a0);
  WOLASynthesisFilter_Float_M64_m4_r1 = ABSOLUTE(0x104da0);
  WOLAAnalysisFilter_Float_M128_m4_r1 = ABSOLUTE(0x1051a0);
  WOLASynthesisFilter_Float_M128_m4_r1 = ABSOLUTE(0x1059a0);
  WOLAAnalysisFilter_Float_M256_m4_r1 = ABSOLUTE(0x1061a0);
  WOLASynthesisFilter_Float_M256_m4_r1 = ABSOLUTE(0x1071a0);
  quantize_intervals_LF = ABSOLUTE(0x10d760);
  invert_quantize_dither_factors_LF = ABSOLUTE(0x10d864);
  quantize_dither_factors_LF = ABSOLUTE(0x10d968);
  quantize_factor_select_offset_LF = ABSOLUTE(0x10da6c);
  hd_quantize_intervals_LF = ABSOLUTE(0x10daf0);
  boot_arg_list = ABSOLUTE(0x2bc04);
  _IRQ_MEM_ADDR = ABSOLUTE(0x2bf00);
  _MASK_MEM_BEGIN = ABSOLUTE(0x2bbe0);
  _MASK_MEM_SIZE = ABSOLUTE(0x308);

EXTERN(




tws_local_media_sync
tws_ci_data
tws_acl_data_sync
tws_event_sync
tws_conn_sync
tws_lmp_slot_sync
tws_media_sync
tws_sync_call
tws_link_sync
tws_afh_sync
tws_power_balance
sdfile_vfs_ops
sbc_decoder
msbc_decoder
sbc_hwaccel
cvsd_decoder
pcm_decoder
aac_decoder







wtgv2_decoder
);

UPDATA_SIZE = 0x80;
UPDATA_BEG = _MASK_MEM_BEGIN - UPDATA_SIZE;




UPDATA_BREDR_BASE_BEG = 0x2c000;


RAM_LIMIT_L = 0;
RAM_LIMIT_H = UPDATA_BEG;
PHY_RAM_SIZE = RAM_LIMIT_H - RAM_LIMIT_L;


ISR_BASE = _IRQ_MEM_ADDR;
ROM_RAM_SIZE = _MASK_MEM_SIZE;
ROM_RAM_BEG = _MASK_MEM_BEGIN;

RAM0_BEG = RAM_LIMIT_L;
RAM0_END = RAM_LIMIT_H;
RAM0_SIZE = RAM0_END - RAM0_BEG;



EQ_PRIV_COEFF_BASE = 0x02D860;
EQ_PRIV_SECTION_NUM = 10;
EQ_PRIV_COEFF_END = EQ_PRIV_COEFF_BASE + 4 * EQ_PRIV_SECTION_NUM * 14;
CODE_BEG = 0x1E00100;




MEMORY
{

 code0(rx) : ORIGIN = CODE_BEG , LENGTH = (1024 * 1024)



 ram0(rwx) : ORIGIN = RAM0_BEG, LENGTH = RAM0_SIZE
}


ENTRY(_start)

SECTIONS
{

    . = ORIGIN(ram0);

    .mmu_tlb ALIGN(0x4000):
    {
        *(.mmu_tlb_segment);
    } > ram0

 .boot_info ALIGN(32):
 {
  *(.boot_info)
        . = ALIGN(32);
 } > ram0

 .irq_stack ALIGN(32):
    {
        _cpu0_sstack_begin = .;
        PROVIDE(cpu0_sstack_begin = .);
        *(.stack)
        _cpu0_sstack_end = .;
        PROVIDE(cpu0_sstack_end = .);
     _stack_end = . ;
  . = ALIGN(4);

    } > ram0

 .data ALIGN(32):
 {

        . = ALIGN(4);
        *(.data_magic)

  *(.audio_dec_data)
  . = ALIGN(4);


 . = ALIGN(4);
 audio_sync_data_begin = .;
        *(.audio_sync_data)
 audio_sync_data_end = .;
 . = ALIGN(4);




 . = ALIGN(4);

    *(.jlsp_data)
    *(.sbc_data)
    *(.msbc_data)
    *(.cvsd_data)
    *(.aac_data)
 *(.sbc_eng_data)
 *(.bt_audioplc_data)
    *(.lf_audioplc_data)
 . = ALIGN(4);

  *(.wav_data)
  *(.wav_dec_data)
  *(.wav_mem)
  *(.wav_ctrl_mem)
  . = ALIGN(4);

        *(.data*)
        *(*.data)

  . = ALIGN(4);
        __a2dp_movable_slot_start = .;
        *(.movable.slot.1);
        __a2dp_movable_slot_end = .;

  . = ALIGN(4);
        __esco_movable_slot_start = .;
        *(.movable.slot.2);
        __esco_movable_slot_end = .;

        . = ALIGN(32);


        btstack_data_start = .;
        *(.bt_stack_data)
        *(.ble_db_data)
        *(.ble_sm_data)
        *(.ble_att_data)
        *(.ble_gatt_data)


        BTSTACK_LE_HOST_MESH_DATA_START = .;
  . = (. +3) & ~ 3;
        _net_buf_pool_list = .;
        *(._net_buf_pool.static.*)

        *(.ble_mesh_data)
        *(.ble_mesh_tinycrypt_data)
        BTSTACK_LE_HOST_MESH_DATA_SIZE = ABSOLUTE(. - BTSTACK_LE_HOST_MESH_DATA_START);


        btstack_data_end = .;
 . = ALIGN(4);


  . = ALIGN(4);
        system_data_start = .;

  _video_subdev_begin = .;
  PROVIDE(video_subdev_begin = .);
  KEEP(*(.video_subdev.0))
  KEEP(*(.video_subdev.1))
  KEEP(*(.video_subdev.2))
  KEEP(*(.video_subdev.3))
  KEEP(*(.video_subdev.4))
  KEEP(*(.video_subdev.5))
  _video_subdev_end = .;
  PROVIDE(video_subdev_end = .);

  _audio_subdev_begin = .;
  PROVIDE(audio_subdev_begin = .);
  KEEP(*(.audio_subdev.0))
  KEEP(*(.audio_subdev.1))
  KEEP(*(.audio_subdev.2))
  KEEP(*(.audio_subdev.3))
  _audio_subdev_end = .;
  PROVIDE(audio_subdev_end = .);

  _iic_device_begin = .;
  PROVIDE(iic_device_begin = .);
  KEEP(*(.iic))
  _iic_device_end = .;
  PROVIDE(iic_device_end = .);

        _avin_spi_device_begin = .;
  PROVIDE(avin_spi_device_begin = .);
  KEEP(*(.sw_spi))
  _avin_spi_device_end = .;
  PROVIDE(avin_spi_device_end = .);

  _video_dev_begin = .;
  PROVIDE(video_dev_begin = .);
  KEEP(*(.video_device))
  _video_dev_end = .;
  PROVIDE(video_dev_end = .);

  _key_driver_ops_begin = .;
  PROVIDE(key_driver_ops_begin = .);
  KEEP(*(.key_driver_ops))
  _key_driver_ops_end = .;
  PROVIDE(key_driver_ops_end = .);

  _touch_driver_begin = .;
  PROVIDE(touch_driver_begin = .);
  KEEP(*(.touch_driver))
  _touch_driver_end = .;
  PROVIDE(touch_driver_end = .);

  _static_hi_timer_begin = .;
  PROVIDE(static_hi_timer_begin = .);
  KEEP(*(.hi_timer))
  _static_hi_timer_end = .;
  PROVIDE(static_hi_timer_end = .);

  _sys_cpu_timer_begin = .;
  PROVIDE(sys_cpu_timer_begin = .);
  KEEP(*(.sys_cpu_timer))
  _sys_cpu_timer_end = .;
  PROVIDE(sys_cpu_timer_end = .);

     _sys_config_begin = .;
     PROVIDE(sys_config_begin = .);
     KEEP(*(.sys_cfg))
     _sys_config_end = .;
     PROVIDE(sys_config_end = .);

     _sys_fat_begin = .;
     PROVIDE(sys_fat_begin = .);
     KEEP(*(.fs_fat))
     _sys_fat_end = .;
     PROVIDE(sys_fat_end = .);

  _app_begin = .;
  PROVIDE(app_begin = .);
  KEEP(*(.app))
  _app_end = .;
  PROVIDE(app_end = .);

  _os_begin = .;
  PROVIDE(os_begin = .);




     *(.os_str)
     *(.os_data)
  _os_end = .;
  PROVIDE(os_end = .);

     *(.crypto_ecdh_data)
     *(.crypto_data)

     *(.mem_data)
        *(.os_port_data)

     *(.uECC_data)
     *(.ECDH_sample_data)

        system_data_end = .;
 . = ALIGN(4);
 } > ram0

 .bss ALIGN(32):
    {
        . = ALIGN(4);


        btstack_bss_start = .;
        *(.bt_stack_bss)
        *(.ble_db_bss)
        *(.ble_sm_bss)
        *(.ble_att_bss)
        *(.ble_gatt_bss)
        *(.btstack_pool)


        BTSTACK_LE_HOST_MESH_BSS_START = .;
        *(.ble_mesh_bss)
        *(.ble_mesh_tinycrypt_bss)
        BTSTACK_LE_HOST_MESH_BSS_SIZE = ABSOLUTE(. - BTSTACK_LE_HOST_MESH_BSS_START);

        btstack_bss_end = .;
 . = ALIGN(4);


        system_bss_start = .;
        . = ALIGN(4);
        *(.os_bss)
        *(.mem_heap)
  *(.memp_memory_x)
        *(.mem_bss)
        *(.os_port_bss)

        *(.uECC_bss)
        *(.ECDH_sample_bss)

        system_bss_end = .;
 . = ALIGN(4);
        *(.bss)
        . = ALIGN(4);


 . = ALIGN(4);
 audio_sync_bss_begin = .;
        *(.audio_sync_bss)
 audio_sync_bss_end = .;
 . = ALIGN(4);

    *(.jlsp_bss)
    *(.cvsd_bss)
    *(.aac_bss)
 *(.sbc_eng_bss)
 *(.bt_audioplc_bss)
    *(.lf_audioplc_bss)
 . = ALIGN(4);

        *(*.data.bss)

  *(.wav_bss)
  *(.wav_dec_bss)
        . = ALIGN(4);

  *(.audio_dec_bss)
        *(.os_bss)
        *(.volatile_ram)
  *(.btstack_pool)
        *(.usb_ep0)
        *(.usb_msd_dma)
  *(.audio_buf)
        *(.src_filt)
        *(.src_dma)
        *(.mem_heap)
  *(.memp_memory_x)

        . = ALIGN(4);
  *(.non_volatile_ram)

        . = ALIGN(32);
    } > ram0

 .data_code ALIGN(32):
 {
  data_code_pc_limit_begin = .;
  *(.flushinv_icache)
        *(.cache)
        *(.os_critical_code)
        *(.volatile_ram_code)
    *(.media.aec.text)
  *(.os_code)
  *(.os_str)
        *(.movable.stub.1)
        *(.movable.stub.2)
        *(*.text.cache.L1)
        *(*.text.const.cache.L2)



  *(.mixer.text.cache.L2.run)
  *(.audio_eq.text.cache.L2.eq)
  *(.eq_codec.text.cache.L2.hweq)
  *(.audio_dac.text.cache.L2.dac)
  *(.audio_syncts.text.cache.L2.runtime)
  *(.audio_sync.text.cache.L2.src)
  *(.audio_sync.text.cache.L2.hw)
  *(.sbc_decoder.text.cache.L2.run)
  *(.audio_sbc_hw.text.cache.L2.run)
  *(.bt_decode.text.cache.L2.decoder)
  *(.bt_decode.text.cache.L2.a2dp)
  *(.bt_decode.text.cache.L2.mixer)
  *(.bt_audio_timestamp.text.cache.L2.a2dp)




        *(.tws.text.cache.L2.irq)
        *(.lmp.text.cache.L2.irq)
        *(.bt_classic.text.cache.L2.irq)

  . = ALIGN(4);
        tws_sync_channel_begin = .;
        *(.tws.text.cache.L1.sync_channel0)
        *(.tws.text.cache.L1.sync_channel1)
        *(.tws.text.cache.L1.sync_channel2)
        *(.tws.text.cache.L1.sync_channel3)
        *(.tws.text.cache.L1.sync_channel4)
        *(.tws.text.cache.L1.sync_channel5)
        *(.tws.text.cache.L1.sync_channel6)
        *(.tws.text.cache.L1.sync_channel7)
        *(.tws.text.cache.L1.sync_channel8)
        *(.tws.text.cache.L1.sync_channel9)
        *(.tws.text.cache.L1.sync_channel10)
        *(.tws.text.cache.L1.sync_channel11)
        *(.tws.text.cache.L1.sync_channel12)
        *(.tws.text.cache.L1.sync_channel13)
        tws_sync_channel_end = .;


        *(.fat_data_code)


  . = ALIGN(4);
        data_code_pc_limit_end = .;
 } > ram0
 __report_overlay_begin = .;
 OVERLAY : AT(0x200000) SUBALIGN(4)
    {
  .overlay_aec
  {
            . = ALIGN(4);
   aec_code_begin = . ;
   *(.text._*)
   *(.data._*)
   *(.aec_code)
   *(.aec_const)
   *(.res_code)
   *(.res_const)
            *(.dns_common_data)
            *(.dns_param_data_single)
            *(.dns_param_data_dual)
   *(.ns_code)
   *(.ns_const)
   *(.fft_code)
   *(.fft_const)
   *(.nlp_code)
   *(.nlp_const)
   *(.der_code)
   *(.der_const)
   *(.qmf_code)
   *(.qmf_const)
   *(.aec_data)
   *(.res_data)
   *(.ns_data)
   *(.nlp_data)
         *(.der_data)
         *(.qmf_data)
         *(.fft_data)
   *(.dms_code)
   *(.dms_const)
   *(.dms_data)

   *(.audio_cvp.text.cache.L2.cvp)

            . = ALIGN(4);
   *(.aec_mux)
            . = ALIGN(4);
   aec_code_end = . ;
   aec_code_size = aec_code_end - aec_code_begin ;
  }

  .overlay_aac
  {
   . = ALIGN(4);
   aac_dec_code_begin = .;
   *(.bt_aac_dec_code)
            *(.bt_aac_dec_sparse_code)
   aac_dec_code_end = .;
   aac_dec_code_size = aac_dec_code_end - aac_dec_code_begin ;

   . = ALIGN(4);
   bt_aac_dec_const_begin = .;
   *(.bt_aac_dec_const)
            *(.bt_aac_dec_sparse_const)
   . = ALIGN(4);
   bt_aac_dec_const_end = .;
   bt_aac_dec_const_size = bt_aac_dec_const_end - bt_aac_dec_const_begin ;

   *(.bt_aac_dec_data)
   LONG(0xFFFFFFFF);
   . = ALIGN(4);
   *(.bt_aac_dec_bss)
   . = ALIGN(4);
   *(.aac_mem)
   *(.aac_ctrl_mem)
  }

    } > ram0


    .ram0_empty0 ALIGN(4) :
 {
        . = . + 4;
    } > ram0


 OVERLAY : AT(0x210000) SUBALIGN(4)
    {
  .overlay_aec_ram
  {
            . = ALIGN(4);
   *(.msbc_enc)
   *(.cvsd_codec)
   *(.aec_bss)
   *(.res_bss)
   *(.ns_bss)
   *(.nlp_bss)
         *(.der_bss)
         *(.qmf_bss)
         *(.fft_bss)
   *(.aec_mem)
   *(.dms_bss)
  }

        .overlay_opus
        {
            *(.opus_mem)
        }

  .overlay_mp3
  {
   *(.mp3_mem)
   *(.mp3_ctrl_mem)
   *(.mp3pick_mem)
   *(.mp3pick_ctrl_mem)
   *(.dec2tws_mem)
  }
  .overlay_wma
  {
   *(.wma_mem)
   *(.wma_ctrl_mem)
   *(.wmapick_mem)
   *(.wmapick_ctrl_mem)
  }







  .overlay_ape
        {
            *(.ape_mem)
            *(.ape_ctrl_mem)
  }
  .overlay_flac
        {
            *(.flac_mem)
            *(.flac_ctrl_mem)
  }
  .overlay_m4a
        {
            *(.m4a_mem)
            *(.m4a_ctrl_mem)
  }
  .overlay_amr
        {
            *(.amr_mem)
            *(.amr_ctrl_mem)
  }
  .overlay_dts
        {
            *(.dts_mem)
            *(.dts_ctrl_mem)
  }
  .overlay_fm
  {
   *(.fm_mem)
  }
        .overlay_pc
  {
            *(.usb_audio_play_dma)
            *(.usb_audio_rec_dma)
            *(.uac_rx)
            *(.mass_storage)

            *(.usb_hid_dma)
            *(.usb_iso_dma)
            *(.uac_var)
            *(.usb_config_var)
  }

    } > ram0


 __report_overlay_end = .;



 _HEAP_BEGIN = . ;
 _HEAP_END = RAM0_END;


    . = ORIGIN(code0);
    .text ALIGN(4):
    {
        PROVIDE(text_rodata_begin = .);

        *(.startup.text)

  *(.text)
  *(*.text)
        *(*.text.cache.L2)
        *(*.text.cache.L2.*)

        . = ALIGN(4);
  *(*.text.const)


  . = ALIGN(4);
        tws_sync_call_begin = .;
        KEEP(*(.tws.text.sync_call))
        tws_sync_call_end = .;

  . = ALIGN(4);
        tws_func_stub_begin = .;
        KEEP(*(.tws.text.func_stub))
        tws_func_stub_end = .;





        btstack_code_start = .;

        . = ALIGN(4);

        a2dp_source_media_codec_begin = .;
        KEEP(*(.a2dp_source_media_codec))
            a2dp_source_media_codec_end = .;
        a2dp_sink_media_probe_begin = .;
        KEEP(*(.a2dp_sink_media_probe))
            a2dp_sink_media_probe_end = .;

        a2dp_sink_media_codec_begin = .;
        KEEP(*(.a2dp_sink_media_codec))
            a2dp_sink_media_codec_end = .;

        a2dp_event_handler_begin = .;
        KEEP(*(.a2dp_event_handler))
            a2dp_event_handler_end = .;

        sdp_record_item_begin = .;
        KEEP(*(.sdp_record_item))
            sdp_record_item_end = .;

        bt_sleep_begin = .;
        KEEP(*(.bt_sleep))
            bt_sleep_end = .;

  *(.bt_stack_const)
  *(.bt_stack_code)
        *(.ble_db_const)
        *(.ble_db_code)
        *(.ble_sm_const)
        *(.ble_sm_code)
        *(.ble_att_const)
        *(.ble_att_code)
        *(.ble_gatt_const)
        *(.ble_gatt_code)


        BTSTACK_LE_HOST_MESH_CODE_START = .;
        *(.ble_mesh_code)
        *(.ble_mesh_tinycrypt_code)

        *(.ble_mesh_const)
        *(.ble_mesh_tinycrypt_const)
        BTSTACK_LE_HOST_MESH_CODE_SIZE = ABSOLUTE(. - BTSTACK_LE_HOST_MESH_CODE_START);

        btstack_code_end = .;
  . = ALIGN(4);


        BTSTACK_CODE_TOTAL_SIZE = btstack_code_end - btstack_code_start;

  . = ALIGN(4);
        system_text_start = .;

   _device_node_begin = .;
     PROVIDE(device_node_begin = .);
     KEEP(*(.device))
     _device_node_end = .;
     PROVIDE(device_node_end = .);

  config_target_begin = .;
     PROVIDE(config_target_begin = .);
     KEEP(*(.config_target))
  config_target_end = .;
     PROVIDE(config_target_end = .);

     system_code_begin = .;
     KEEP(*(.system.*.text))
     system_code_end = .;
  . = ALIGN(4);
  system_code_size = system_code_end - system_code_begin;

  vfs_ops_begin = .;
  KEEP(*(.vfs_operations))
  vfs_ops_end = .;

     _lib_version_begin = .;
     PROVIDE(lib_version_begin = .);
     KEEP(*(.lib_version))
     _lib_version_end = .;
     PROVIDE(lib_version_end = .);

  _initcall_begin = .;
  PROVIDE(initcall_begin = .);
     KEEP(*(.initcall))
  _initcall_end = .;
  PROVIDE(initcall_end = .);

  _early_initcall_begin = .;
  PROVIDE(early_initcall_begin = .);
  KEEP(*(.early.initcall))
  _early_initcall_end = .;
  PROVIDE(early_initcall_end = .);

  _late_initcall_begin = .;
  PROVIDE(late_initcall_begin = .);
  KEEP(*(.late.initcall))
  _late_initcall_end = .;
  PROVIDE(late_initcall_end = .);

  _platform_initcall_begin = .;
  PROVIDE(platform_initcall_begin = .);
  KEEP(*(.platform.initcall))
  _platform_initcall_end = .;
  PROVIDE(platform_initcall_end = .);

  _module_initcall_begin = .;
  PROVIDE(module_initcall_begin = .);
  KEEP(*(.module.initcall))
  _module_initcall_end = .;
  PROVIDE(module_initcall_end = .);

  _sys_event_handler_begin = .;
  PROVIDE(sys_event_handler_begin = .);
  KEEP(*(.sys_event.4.handler))
  KEEP(*(.sys_event.3.handler))
  KEEP(*(.sys_event.2.handler))
  KEEP(*(.sys_event.1.handler))
  KEEP(*(.sys_event.0.handler))
  _sys_event_handler_end = .;
  PROVIDE(sys_event_handler_end = .);

  _syscfg_arg_begin = .;
  PROVIDE(syscfg_arg_begin = .);
  KEEP(*(.syscfg.arg))
  _syscfg_arg_end = .;
  PROVIDE(syscfg_arg_end = .);

  _syscfg_handler_begin = .;
  PROVIDE(syscfg_handler_begin = .);
  KEEP(*(.syscfg.handler))
  _syscfg_handler_end = .;
  PROVIDE(syscfg_handler_end = .);

  _syscfg_ops_begin = .;
  PROVIDE(syscfg_ops_begin = .);
  KEEP(*(.syscfg.2.ops))
  KEEP(*(.syscfg.1.ops))
  KEEP(*(.syscfg.0.ops))
  _syscfg_ops_end = .;
  PROVIDE(syscfg_ops_end = .);

  _server_info_begin = .;
  PROVIDE(server_info_begin = .);
  KEEP(*(.server_info))
  _server_info_end = .;
  PROVIDE(server_info_end = .);

  _bus_device_begin = .;
  PROVIDE(bus_device_begin = .);
  KEEP(*(.bus_device))
  _bus_device_end = .;
  PROVIDE(bus_device_end = .);

     _sys_power_hal_ops_begin = .;
     PROVIDE(sys_power_hal_ops_begin = .);
     KEEP(*(.sys_power_hal_ops))
     _sys_power_hal_ops_end = .;
     PROVIDE(sys_power_hal_ops_end = .);


  . = ALIGN(4);
     lp_target_begin = .;
     PROVIDE(lp_target_begin = .);
     KEEP(*(.lp_target))
     lp_target_end = .;
     PROVIDE(lp_target_end = .);

  . = ALIGN(4);
        deepsleep_target_begin = .;
        PROVIDE(deepsleep_target_begin = .);
        KEEP(*(.deepsleep_target))
        deepsleep_target_end = .;
        PROVIDE(deepsleep_target_end = .);

     crypto_begin = .;
     *(.crypto_ecdh_code)
     *(.crypto_ecdh_const)

     *(.crypto_bigint_code)
     *(.crypto_bigint_const)

     *(.crypto_code)
     *(.crypto_const)

     *(.ECDH_sample_code)
     *(.ECDH_sample_const)

     *(.uECC_code)
     *(.uECC_const)

     *(.hmac_code)
     *(.hmac_const)

     *(.hash_sample_code)
     *(.hash_sample_const)

     *(.aes_cmac_sample_code)
     *(.aes_cmac_sample_const)
     crypto_end = .;
     crypto_size = . - crypto_begin;

        *(.mem_code)
        *(.mem_const)

        *(.os_port_code)
        *(.os_port_const)

        system_text_end = .;



        system_code_total_size = system_text_end - system_text_start;

  . = ALIGN(4);
     update_target_begin = .;
     PROVIDE(update_target_begin = .);
     KEEP(*(.update_target))
     update_target_end = .;
     PROVIDE(update_target_end = .);
  . = ALIGN(4);


  *(.LOG_TAG_CONST*)
        *(.rodata*)

  . = ALIGN(4);
        PROVIDE(text_rodata_end = .);

        clock_critical_handler_begin = .;
        KEEP(*(.clock_critical_txt))
        clock_critical_handler_end = .;

        gsensor_dev_begin = .;
        KEEP(*(.gsensor_dev))
        gsensor_dev_end = .;


  . = ALIGN(4);
  OMSensor_dev_begin = .;
  KEEP(*(.omsensor_dev))
  OMSensor_dev_end = .;

        fm_dev_begin = .;
        KEEP(*(.fm_dev))
        fm_dev_end = .;

        fm_emitter_dev_begin = .;
        KEEP(*(.fm_emitter_dev))
        fm_emitter_dev_end = .;

  . = ALIGN(4);
  tool_interface_begin = .;
  KEEP(*(.tool_interface))
  tool_interface_end = .;

        storage_device_begin = .;
        KEEP(*(.storage_device))
        storage_device_end = .;

  . = ALIGN(4);
  ai_server_device_begin = .;
  KEEP(*(.ai_server_device))
  ai_server_device_end = .;



        *(.opcore_table_maskrom)
        *(.bfilt_table_maskroom)
        *(.opcore_maskrom)
        *(.bfilt_code)
        *(.bfilt_const)


  . = ALIGN(4);


  . = ALIGN(4);
        media_text_start = .;

  . = ALIGN(4);
        *(.dns_16k_data)
        *(.dns_8k_data)
  *(.jlsp_const)
  *(.jlsp_code)

  *(.audio_gain_code)
  *(.audio_gain_const)
  *(.ns_sparse_code )
  *(.aec_sparse_code)
  *(.nlp_sparse_code)
  *(.der_sparse_code)
  *(.qmf_sparse_code)
  *(.bt_audioplc_code)
  *(.bt_audioplc_sparse_code)
  *(.bt_audioplc_const)
  *(.dms_sparse_code)
         *(.bark_const)

  . = ALIGN(4);
        *(.pcm_code)
        *(.pcm_const)
        *(.cvsd_code)
        *(.cvsd_const)
        *(.g729_code)
        *(.g729_const)
  *(.wtgv2_code)
  *(.wtgv2_const)
  *(.wtgv2dec_code)
  *(.wtgv2dec_const)
  *(.wtgv2dec_str)
  *(.wtg_decv2_sparse_code)
        *(.mp3_code)
        *(.mp3_const)
        *(.mp3_dec_code)
        *(.mp3_dec_const)
        *(.mp3_dec_sparse_code)

        *(.mp3_decstream_code)
        *(.mp3_decstream_const)
        *(.mp3_decstream_sparse_code)

  *(.msbc_code)
  *(.msbc_const)


  *(.sbc_code)
  *(.sbc_const)
  *(.sine_code)
  *(.sine_const)
        *(.wav_code)
        *(.wav_const)
  *(.wav_dec_code)
  *(.wav_dec_const)
  *(.wav_dec_sparse_code)
  *(.wav_dec_sparse_const)
        *(.wma_code)
        *(.wma_const)
  *(.wma_dec_code)
  *(.wma_dec_const)
  *(.wma_dec_sparse_code)
  *(.wma_dec_sparse_const)
        *(.wma_decstream_code)
        *(.wma_decstream_const)
        *(.wma_decstream_sparse_code)
     audio_sync_code_begin = .;
        *(.audio_sync_code)
     audio_sync_code_end = .;

        *(.aac_code)
        *(.aac_const)
  *(.amr_code)
  *(.amr_const)


  *(.audio_decoder_code)
  *(.audio_decoder_const)
  *(.media_device_code)
  *(.media_device_const)
  *(.audio_encoder_code)
  *(.audio_encoder_const)
  *(.mixer_code)
  *(.mixer_const)
  *(.dec_server_code)
  *(.dec_server_const)
  *(.rec_server_code)
  *(.rec_server_const)
  *(.auto_mute_code)
  *(.auto_mute_const)
  *(.plc_code)
  *(.plc_const)
  *(.wireless_sync_code)
  *(.wireless_sync_const)
  *(.sbc_eng_code)
  *(.sbc_eng_const)

  *(.audio_codec_code)

  *(.bt_compressor_sparse_const)
  *(.bt_compressor_sparse_code)
  *(.compressor_sparse_code)
  *(.compressor_sparse_const)

  *(.bt_limiter_sparse_const)
  *(.bt_limiter_sparse_code)
  *(.limiter_sparse_code)
  *(.limiter_sparse_const)

  *(.bt_crossOver_sparse_const)
  *(.bt_crossOver_sparse_code)
  *(.crossOver_sparse_code)
  *(.crossOver_sparse_const)

  . = ALIGN(4);
     _audio_decoder_begin = .;
     PROVIDE(audio_decoder_begin = .);
         *(.audio_decoder)
     _audio_decoder_end = .;
     PROVIDE(audio_decoder_end = .);

     _audio_encoder_begin = .;
     PROVIDE(audio_encoder_begin = .);
         *(.audio_encoder)
     _audio_encoder_end = .;
     PROVIDE(audio_encoder_end = .);

     _audio_package_begin = .;
     PROVIDE(audio_package_begin = .);
         *(.audio_package)
     _audio_package_end = .;
     PROVIDE(audio_package_end = .);

     _audio_dev_begin = .;
     PROVIDE(audio_dev_begin = .);
         *(.audio_device)
     _audio_dev_end = .;
     PROVIDE(audio_dev_end = .);

     _audio_hwaccel_begin = .;
     PROVIDE(audio_hwaccel_begin = .);
         *(.audio_hwaccel)
     _audio_hwaccel_end = .;
     PROVIDE(audio_hwaccel_end = .);

  . = ALIGN(4);
     media_code_begin = .;
         *(.media.*.text)
  . = ALIGN(4);
  *(.compressor_code)
  *(.compressor_const)
  *(.bt_compressor_const)
  *(.bt_compressor_code)

  *(.crossOver_code)
  *(.crossOver_const)
  *(.bt_crossOver_const)
  *(.bt_crossOver_code)

  *(.limiter_code)
  *(.limiter_const)
  *(.bt_limiter_const)
  *(.bt_limiter_code)
 . = ALIGN(4);
  *(.sur_cal_const)
  *(.sur_cal_code)
 . = ALIGN(4);
 *(.lib_pitchshift_code)
 *(.lib_pitchshift_const)
 . = ALIGN(4);
        *(.lf_audioplc_sparse_code)
        *(.lf_audioplc_code)
        *(.lf_audioplc_const)
 . = ALIGN(4);
     *(.drc_const)
  *(.drc_code)
  *(.drc_sparse_code)
  *(.drc_sparse_const)
 . = ALIGN(4);

     media_code_end = .;
  . = ALIGN(4);
  media_code_size = media_code_end - media_code_begin;

  . = ALIGN(4);
        media_text_end = .;


  . = ALIGN(4);
        __VERSION_BEGIN = .;
        KEEP(*(.sys.version))
        __VERSION_END = .;

        *(.noop_version)

  . = ALIGN(4);
  *( .wtg_dec_code )
  *( .wtg_dec_const)
  *( .wtg_dec_sparse_code)

  . = ALIGN(4);
        _SPI_CODE_START = . ;
        *(.spi_code)
  . = ALIGN(4);
        _SPI_CODE_END = . ;

  . = ALIGN(4);
        __a2dp_text_cache_L2_start = .;
        *(.movable.region.1);
  . = ALIGN(4);
        __a2dp_text_cache_L2_end = .;

  . = ALIGN(4);
        __esco_text_cache_L2_start = .;
        *(.movable.region.2);
  . = ALIGN(4);
        __esco_text_cache_L2_end = .;

  . = ALIGN(4);

   } > code0
}




SECTIONS
{
    .data : ALIGN(4)
    {
        update_data_start = .;

        update_data_end = .;
    } > ram0

    .bss (NOLOAD) :ALIGN(4)
    {
        update_bss_start = .;

        *(.update_bss)
        update_bss_end = .;
    } > ram0

    .text : ALIGN(4)
    {
  update_code_start = .;

  *(.bt_updata_ram_code)
  *(.update_const)
  *(.update_code)

  update_code_end = .;
    } > code0


    UPDATE_CODE_TOTAL_SIZE = update_code_end - update_code_start;
}
SECTIONS
{
    .data : ALIGN(4)
    {
        driver_data_start = .;


        CLOCK_DATA_START = .;
        *(.clock_data)
        CLOCK_DATA_SIZE = ABSOLUTE(. - CLOCK_DATA_START);

        *(.debug_data)
        *(.power_data)

        *(.uart_data)

        driver_data_end = .;
        . = ALIGN(32);
    } > ram0

    .bss (NOLOAD) :ALIGN(4)
    {
        driver_bss_start = .;

        CLOCK_BSS_START = .;
        *(.clock_bss)
        CLOCK_BSS_SIZE = ABSOLUTE(. - CLOCK_BSS_START);

        *(.debug_bss)
        *(.power_bss)

        *(.uart_bss)

        *(.sd_var)
        driver_bss_end = .;
    } > ram0

    .text : ALIGN(4)
    {
        driver_code_start = .;

        *(.fm_code)

        CLOCK_CODE_START = .;
        *(.clock_code)
        *(.clock_const)
        CLOCK_CODE_SIZE = ABSOLUTE(. - CLOCK_CODE_START);

        *(.debug_code)
        *(.debug_const)

        *(.power_code)
        *(.power_const)

        *(.uart_code)
        *(.uart_const)

        driver_code_end = .;
  . = ALIGN(4);
    } > code0


    DRIVER_RAM_TOTAL = (driver_data_end - driver_data_start) + (driver_bss_end - driver_bss_start);
    DRIVER_CODE_TOTAL = (driver_code_end - driver_code_start);
}

text_begin = ADDR(.text);
text_size = SIZEOF(.text);
text_end = text_begin + text_size;

bss_begin = ADDR(.bss);
bss_size = SIZEOF(.bss);
bss_end = bss_begin + bss_size;

data_addr = ADDR(.data);
data_begin = text_begin + text_size;
data_size = SIZEOF(.data);

data_code_addr = ADDR(.data_code);
data_code_begin = data_begin + data_size;
data_code_size = SIZEOF(.data_code);



aec_addr = ADDR(.overlay_aec);
aec_begin = data_code_begin + data_code_size;
aec_size = SIZEOF(.overlay_aec);

aac_addr = ADDR(.overlay_aac);
aac_begin = aec_begin + aec_size;
aac_size = SIZEOF(.overlay_aac);




PROVIDE(HEAP_BEGIN = _HEAP_BEGIN);
PROVIDE(HEAP_END = _HEAP_END);
_MALLOC_SIZE = _HEAP_END - _HEAP_BEGIN;
PROVIDE(MALLOC_SIZE = _HEAP_END - _HEAP_BEGIN);

ASSERT(MALLOC_SIZE >= 0x8000, "heap space too small !")




report_text_beign = ADDR(.text);
report_text_size = SIZEOF(.text);
report_text_end = report_text_beign + report_text_size;

report_mmu_tlb_begin = ADDR(.mmu_tlb);
report_mmu_tlb_size = SIZEOF(.mmu_tlb);
report_mmu_tlb_end = report_mmu_tlb_begin + report_mmu_tlb_size;

report_boot_info_begin = ADDR(.boot_info);
report_boot_info_size = SIZEOF(.boot_info);
report_boot_info_end = report_boot_info_begin + report_boot_info_size;

report_irq_stack_begin = ADDR(.irq_stack);
report_irq_stack_size = SIZEOF(.irq_stack);
report_irq_stack_end = report_irq_stack_begin + report_irq_stack_size;

report_data_begin = ADDR(.data);
report_data_size = SIZEOF(.data);
report_data_end = report_data_begin + report_data_size;

report_bss_begin = ADDR(.bss);
report_bss_size = SIZEOF(.bss);
report_bss_end = report_bss_begin + report_bss_size;

report_data_code_begin = ADDR(.data_code);
report_data_code_size = SIZEOF(.data_code);
report_data_code_end = report_data_code_begin + report_data_code_size;

report_overlay_begin = __report_overlay_begin;
report_overlay_size = __report_overlay_end - __report_overlay_begin;
report_overlay_end = __report_overlay_end;

report_heap_beign = _HEAP_BEGIN;
report_heap_size = _HEAP_END - _HEAP_BEGIN;
report_heap_end = _HEAP_END;

BR30_PHY_RAM_SIZE = PHY_RAM_SIZE;
BR30_SDK_RAM_SIZE = report_mmu_tlb_size + report_boot_info_size + report_irq_stack_size + report_data_size + report_bss_size + report_overlay_size + report_data_code_size + report_heap_size;
