@echo off

;rem move into the Example Directory
set BATCHDIR=%~dp0%
cd /d %BATCHDIR%

;rem Check Installation supports this demo
if EXIST ..\..\bin\%IMPERAS_ARCH%\checkinstall.exe (
  ..\..\bin\%IMPERAS_ARCH%\checkinstall.exe -p install.pkg --nobanner
  if ERRORLEVEL 1 ( goto end )
)



;rem <app name> <num cores> <stop time>
OVPsim_FreeRTOS_ROSH_arm_CortexM3_AMP_xN.%IMPERAS_ARCH%.exe 4 0

:end
cd UART_LOG
clean_uarts_logs.py
pause
