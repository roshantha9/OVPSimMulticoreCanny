

all: FreeRTOS_all platform


FreeRTOS_all:
	@ $(MAKE) -f Makefile.AllCoreApps all_elfs

FreeRTOS_cleanall:
	@ $(MAKE) -f Makefile.AllCoreApps clean

	
	
FreeRTOS:
	@ $(MAKE) -f Makefile.FreeRTOS
	
platform:
	@ $(MAKE) -f Makefile.platform
	

clean_all:
	@ $(MAKE) -f Makefile.FreeRTOS clean
	@ $(MAKE) -f Makefile.FreeRTOS clean_elfs
	@ $(MAKE) -f Makefile.platform clean
	- rm UART_LOG/*.log
	-rm *.gdb
	-rm PythonTests/images_out/*.*
	
	
clean:
	@ $(MAKE) -f Makefile.FreeRTOS clean
	@ $(MAKE) -f Makefile.platform clean
