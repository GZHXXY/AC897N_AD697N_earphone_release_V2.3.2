

















rem @echo off

@echo *****************************************************************
@echo SDK BR30
@echo *****************************************************************
@echo %date%

cd %~dp0

set OBJDUMP=C:\JL\pi32\bin\llvm-objdump.exe
set OBJCOPY=C:\JL\pi32\bin\llvm-objcopy.exe
set ELFFILE=sdk.elf

REM %OBJDUMP% -D -address-mask=0x1ffffff -print-dbg $1.elf > $1.lst
%OBJCOPY% -O binary -j .text %ELFFILE% text.bin
%OBJCOPY% -O binary -j .data %ELFFILE% data.bin
%OBJCOPY% -O binary -j .data_code %ELFFILE% data_code.bin
%OBJCOPY% -O binary -j .overlay_aec %ELFFILE% aec.bin
%OBJCOPY% -O binary -j .overlay_aac %ELFFILE% aaco.bin

%OBJDUMP% -section-headers -address-mask=0x1ffffff %ELFFILE%
%OBJDUMP% -t %ELFFILE% > symbol_tbl.txt

remove_tailing_zeros -i aaco.bin -o aac.bin -mark ff

copy /b text.bin+data.bin+data_code.bin+aec.bin+aac.bin app.bin
copy br30_p11_code.bin p11_code.bin
copy br30_ota.bin ota.bin
copy br30_ota_debug.bin ota_debug.bin





copy eq_cfg_hw_less.bin eq_cfg_hw.bin
copy app.bin earphone\standard\app.bin
copy br30loader.bin earphone\standard\br30loader.bin
copy br30loader.uart earphone\standard\br30loader.uart

earphone\standard\download.bat AD697N %kws_cfg%
