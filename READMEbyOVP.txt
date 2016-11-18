OVPsim_FreeRTOS_arm

###############################################################################
# Purpose of Demo
###############################################################################
This demo will show the FreeRTOS operating system can be used on the
OVP ARM Cortex-M3 processor model.

The simulation harness OVPsim_FreeRTOS_arm.<OS>.exe was compiled 
using MinGW/MSys http://www.mingw.org under Windows, and GCC under Linux.

###############################################################################
# FreeRTOS Operating System
###############################################################################
This demo is executing a compiled version of FreeRTOS ported to the 
ARM Cortex-M3 processor.

For operating system source and further information please view the FreeRTOS
website www.freertos.org

###############################################################################
# Running the simulation
###############################################################################
Launch the simulation platform by double clicking 'RUN_FreeRTOS.bat' in a Windows
explorer or execute the script 'RUN_FreeRTOS.sh' in a Linux shell.

The simulation will start and a UART Console will be launched. 

The UART Console will print messages and a representation of the LED state will
be printed into the simulation terminal. 

The simulation will stop after 30 seconds of simulated time but can also be 
terminated by closing the UART console.

Statistics will be printed upon completion of the simulation.

###############################################################################
# Application and Platform
###############################################################################

Application -------------------------------------------------------------------

Source found in the directory 'Application' is based upon example code provided
by FreeRTOS to show a minimal system executing. This creates an application 
using the FreeRTOS operating system release version 7.1.1

This is a simple application that creates two tasks and uses the crflash.c 
co-routines.
Each task, Task One and Task two, will relinquish control for a period of time
and then when re-scheduled will access an 'LED' register to toggle the LED state.

The 'LED' access is provided by code in the BoardSupportPackage directory.

The application can be rebuilt using the provided Makefile. This uses the 
FreeRTOS source, version 7.1.1, that is provided with this example.

Platform ----------------------------------------------------------------------

The platform instantiates an ARM Cortex-M model (OVP armm model configured 
as a Cortex-M3) and a peripheral model that provides a single register to control
a bank of LEDs.

###############################################################################
# Re-Building Application and Platform
###############################################################################
         
In order to rebuild a full product install must have been carried out. This may
be the OVP 'OVPsim' package or the Imperas Professional Tools M*SDK 'Imperas' 
package. 
The 'OVPsim' installer is OVPsim.<major version>.<minor version>.<OS>.exe
The 'Imperas' installer is Imperas.<major version>.<minor version>.<OS>.exe

To re-build it is required that a full installation of the OVPsim package 
"OVPsim.<major version>.<minor version>.<OS>.exe" has been performed.

Application -------------------------------------------------------------------

The provided Makefile (Makefile.FreeRTOS) and linker script (standalone.ld) can
be used to rebuild the FreeRTOS demonstration application.

> make FreeRTOS

Platform ----------------------------------------------------------------------

Rebuild the platform using the standard Makefile provided with the OVPsim package 
in either a Linux or an MSYS/MinGW shell.

> make platform

This can also be carried out using the standard Makefile supplied with OVPsim 
directly using the following
 
> make -f ${IMPERAS_HOME}/ImperasLib/buildutils/Makefile.platform \
       NOVLNV=1 \
       SRC=OVPsim_FreeRTOS_arm.c
