#!/bin/sh

# Check Installation supports this demo
if [ -e ../../bin/${IMPERAS_ARCH}/checkinstall.exe ]; then
  ../../bin/${IMPERAS_ARCH}/checkinstall.exe -p install.pkg --nobanner || exit
fi




OVPsim_FreeRTOS_arm.${IMPERAS_ARCH}.exe FreeRTOSDemo.ARM_CORTEX_M3.elf 30   

