@echo off

cd %~dp0

copy ..\..\ota.bin .

..\..\json_to_res.exe json.txt

..\..\md5sum.exe app.bin md5.bin

set /p "themd5=" < "md5.bin"


..\..\isd_download.exe isd_config.ini -tonorflash -dev br30 -boot 0x2000 -div8 -wait 300 -uboot ..\..\uboot.boot -app app.bin  -res cfg_tool.bin  tone.cfg config.dat ..\..\p11_code.bin ..\..\eq_cfg_hw.bin md5.bin -uboot_compress

@REM 常用命令说明
@rem -format vm         // 擦除VM 区域
@rem -format all        // 擦除所有
@rem -reboot 500        // reset chip, valid in JTAG debug


@REM 删除临时文件
if exist *.mp3 del *.mp3 
if exist *.PIX del *.PIX
if exist *.TAB del *.TAB
if exist *.res del *.res
if exist *.sty del *.sty


if exist *.ufw del *.ufw
..\..\ufw_maker.exe -fw_to_ufw jl_isd.fw
copy jl_isd.ufw update_%themd5%.ufw
del jl_isd.ufw

@REM 生成配置文件升级文件
rem ufw_maker.exe -chip AC630N %ADD_KEY% -output config.ufw -res bt_cfg.cfg

