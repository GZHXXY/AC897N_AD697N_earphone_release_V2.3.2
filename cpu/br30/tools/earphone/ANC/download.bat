@echo off


cd %~dp0

copy ..\..\ota.bin .

..\..\isd_download.exe isd_config_%1.ini -tonorflash -dev br30 -boot 0x2000 -div8 -wait 300 -uboot ..\..\uboot.boot -app app.bin  -res cfg_tool.bin tone.cfg ..\..\p11_code.bin ..\..\eq_cfg_hw.bin %2 -uboot_compress

:: -format all
::-reboot 2500

::-format all
::-reboot 100








..\..\ufw_maker.exe -fw_to_ufw jl_isd.fw
copy jl_isd.ufw update.ufw
del jl_isd.ufw

@REM 生成配置文件升级文件
::ufw_maker.exe -chip AC800X %ADD_KEY% -output config.ufw -res bt_cfg.cfg

::IF EXIST jl_693x.bin del jl_693x.bin 


@rem 常用命令说明
@rem -format vm        //擦除VM 区域
@rem -format cfg       //擦除BT CFG 区域
@rem -format 0x3f0-2   //表示从第 0x3f0 个 sector 开始连续擦除 2 个 sector(第一个参数为16进制或10进制都可，第二个参数必须是10进制)


