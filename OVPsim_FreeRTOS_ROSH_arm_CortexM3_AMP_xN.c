/*
 * Copyright (c) 2005-2013 Imperas Software Ltd., www.imperas.com
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied.
 *
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */


////////////////////////////////////////////////////////////////////////////////
//
//                W R I T T E N   B Y   I M P E R A S   I G E N
//
//                          Wed May 23 16:18:27 2012
//
////////////////////////////////////////////////////////////////////////////////


#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "icm/icmCpuManager.h"


#define SIM_ATTRS (ICM_ATTR_TRACE)



#define MIN_ARGS    2
#define MAX_ARGS    3
#define NUMCORES_ARG     1
#define STOPTIME_ARG     2

#define ELF_DIR	"ELF/"

#define DEBUG_CPU  0	// set to  -1 if no debugging

#define WATCH_DEBUG 0


////////////////////////////////////////////////////////////////////////////////
//                                   GLOBALS
////////////////////////////////////////////////////////////////////////////////

// Function Prototypes early declaration
static void parseArgs(int argc, char **argv);
// valid command line
const char *usage = " [numcores] [stoptime]";
// Variables set by arguments
char  *application;               // the application to load
char  *variant     = "Cortex-M3"; // the model variant
int numcores=2;	// default 2 cores
long double stoptime=0.5;


////////////////////////////////////////////////////////////////////////////////
//                                   CALLBACKS
////////////////////////////////////////////////////////////////////////////////

//
// Callback for memory writes to defined external area
//
static ICM_MEM_WRITE_FN(watchWriteCB) {

#if WATCH_DEBUG
    icmPrintf(
        "WATCHPOINT : Writing %d to 0x%08x \n",
		 *((Int32*)value),
        (Int32)address
    );
#endif

    //icmFinish(processor, -7);
}

//
// Callback for memory writes to defined external area
//
static ICM_MEM_READ_FN(watchReadCB) {

#if WATCH_DEBUG
    icmPrintf(
        "WATCHPOINT : Reading %d from 0x%08x \n",
		*((Int32*)value),
        (Int32)address
    );
#endif
    //icmFinish(processor, -7);
}






////////////////////////////////////////////////////////////////////////////////
//                        CREATE VIRTUAL PLATFORM
////////////////////////////////////////////////////////////////////////////////

void createPlatform() {

////////////////////////////////////////////////////////////////////////////////

    //icmInit(ICM_VERBOSE | ICM_STOP_ON_CTRLC | ICM_ENABLE_IMPERAS_INTERCEPTS, 0, 0);
	icmInit(ICM_VERBOSE | ICM_STOP_ON_CTRLC | ICM_ENABLE_IMPERAS_INTERCEPTS | ICM_GDB_CONSOLE, "rsp", 12345);

    icmSetPlatformName("BareMetalArmCortexMFreeRTOS");



	// create an array to hold pointers to processors, busses, memories and other peripherals
    icmProcessorP processor[numcores];
    icmBusP bus[numcores];
    icmMemoryP local[numcores];
	icmPseP UART_p[numcores];
	icmNetP irq_n[numcores];


	// define platform specific constants
	/* Processor Model */
	const char *cpu_model = icmGetVlnvString(
        0,    // path (0 if from the product directory)
        "arm.ovpworld.org",    // vendor
        0,    // library
        "armm",    // name
        0,    // version
        "model"     // model
    );

	/* UART model */
    const char *UART_path = icmGetVlnvString(
        0,    // path (0 if from the product directory)
        0,    // vendor
        0,    // library
        "UartInterface",    // name
        0,    // version
        "pse"     // model
    );


	// model attributes
	//icmAttrListP UART_attr = icmNewAttrList();
	icmAttrListP UART_attr[numcores];




////////////////////////////////////////////////////////////////////////////////
//                               Processors
////////////////////////////////////////////////////////////////////////////////


    icmAttrListP cpu_attr = icmNewAttrList();

    icmAddStringAttr(cpu_attr, "variant", "Cortex-M3");
    icmAddStringAttr(cpu_attr, "UAL", "1");
    icmAddDoubleAttr(cpu_attr, "mips", 12.000000);
    icmAddStringAttr(cpu_attr, "endian", "little");

	const char *armSemihost = icmGetVlnvString(NULL, "arm.ovpworld.org", "semihosting", "armNewlib", "1.0", "model");

	// Create the processor instances
	int stepIndex;
	char nameString[64]; // temporary string to create labels

	/** START of iterative platform elements **/
	for (stepIndex=0; stepIndex < numcores; stepIndex++) {

		////////////////////////////////////////////////////////////////////////////////
		//                               Processors
		////////////////////////////////////////////////////////////////////////////////
		sprintf(nameString, "CPU-%d", stepIndex);
		processor[stepIndex] = icmNewProcessor(
			nameString,   // name
			"armm",   // type
			stepIndex,   // cpuId
			0x0, // flags
			 32,   // address bits
			cpu_model,   // model
			"modelAttrs",   // symbol
			0x20, /*SIM_ATTRS,*/   // procAttrs
			cpu_attr,   // attrlist
			/*armNewlib_0_path*/armSemihost,   // semihost file
			"modelAttrs"    // semihost symbol
		);


		////////////////////////////////////////////////////////////////////////////////
		//                               Processor Buses
		////////////////////////////////////////////////////////////////////////////////
		// create the processor bus
		sprintf(nameString, "BUS-%d", stepIndex);
		bus[stepIndex] = icmNewBus(nameString, 32);

		// connect the processors onto the busses
		icmConnectProcessorBusses(processor[stepIndex], bus[stepIndex], bus[stepIndex]);

		////////////////////////////////////////////////////////////////////////////////
		//                               Local Memory
		////////////////////////////////////////////////////////////////////////////////
		// create memory
		sprintf(nameString, "LOCALMEM-%d", stepIndex);
		local[stepIndex] = icmNewMemory(nameString, 0x7, 0x0fffffff);	// 256 MB

		// connect the memory onto the busses
		icmConnectMemoryToBus(bus[stepIndex], "sp1", local[stepIndex], 0x0);

		////////////////////////////////////////////////////////////////////////////////
		//                               Application Load/Debug
		////////////////////////////////////////////////////////////////////////////////

		// load the application executable file into processor memory space
		sprintf(nameString, "%sFreeRTOSDemo_%d.ARM_CORTEX_M3.elf", ELF_DIR, stepIndex);
		icmLoadProcessorMemory(processor[stepIndex], nameString, 0, 0, 1);

		// mark processor for debugging (negative means no debug)
		if(DEBUG_CPU >= 0){
			if(stepIndex==DEBUG_CPU){
				icmDebugThisProcessor(processor[stepIndex]);
			}
		}

		////////////////////////////////////////////////////////////////////////////////
		//                                  UARTS
		////////////////////////////////////////////////////////////////////////////////

		UART_attr[stepIndex] = icmNewAttrList();

		icmAddUns64Attr(UART_attr[stepIndex], "console", 1);
		icmAddUns64Attr(UART_attr[stepIndex], "finishOnDisconnect", 1);
		icmAddUns64Attr(UART_attr[stepIndex], "loopback", 1);
		sprintf(nameString, "UART_LOG/uart_%d.log", stepIndex);
		icmAddStringAttr(UART_attr[stepIndex], "outfile", nameString);

		sprintf(nameString, "UART%d", stepIndex);
		UART_p[stepIndex] = icmNewPSE(
			nameString,   // name
			UART_path,   // model
			UART_attr[stepIndex],   // attrlist
			0,   // semihost file
			0    // semihost symbol
		);

		icmConnectPSEBus( UART_p[stepIndex], bus[stepIndex], "bport1", 0, 0x1000c000, 0x1000cfff);



	}
	/** END of iterative platform elements **/



////////////////////////////////////////////////////////////////////////////////
//                               WATCHPOINTS
////////////////////////////////////////////////////////////////////////////////
//
// Create a watchpoint
// Invoke callback on write accesses
//
icmAddBusWriteCallback(
	bus[0],
	processor[0],          // processor
	0x40005000,         // low address
	0x80104fff,         // high address
	watchWriteCB,       // callback to invoke
	"" // user data passed to callback
);

//
// Create a watchpoint
// Invoke callback on read accesses
//
icmAddBusReadCallback(
	bus[0],
	processor[0],   // processor
	0x40005000,         	// low address
	0x80104fff,         	// high address
	(icmMemWatchFn)watchReadCB,       // callback to invoke
	"" // user data passed to callback
);


////////////////////////////////////////////////////////////////////////////////
//                              Shared Memory
////////////////////////////////////////////////////////////////////////////////

//icmMemoryP shared_mem_m = icmNewMemory("shared_mem_m", 0x7, 0xbeffffff);
//icmConnectMemoryToBus( bus1_b, "sp1", shared_mem_m, 0x41000000);
//icmConnectMemoryToBus( bus2_b, "sp1", shared_mem_m, 0x41000000);
icmMemoryP shared_mem_m = icmNewMemory("shared_mem_m", ICM_PRIV_RWX, 0x400FFFFF);

/* connect all local buses to shared memory */
for (stepIndex=0; stepIndex < numcores; stepIndex++) {
	sprintf(nameString, "sp%d", stepIndex);
	icmConnectMemoryToBus(bus[stepIndex], nameString, shared_mem_m, 0x40005000);
}


////////////////////////////////////////////////////////////////////////////////
//                              CONNECTIONS
////////////////////////////////////////////////////////////////////////////////

	// create and show all connections
	for (stepIndex=0; stepIndex < numcores; stepIndex++) {
		sprintf(nameString, "irq%d_n", stepIndex);
		irq_n[stepIndex] = icmNewNet(nameString);

		// connect to respective processor
		icmConnectProcessorNet( processor[stepIndex], irq_n[stepIndex], "int", ICM_INPUT);

		// connect uart IRQ
		icmConnectPSENet( UART_p[stepIndex], irq_n[stepIndex], "irq", ICM_OUTPUT);

		// show the bus connections
		icmPrintf("\nbus_%d CONNECTIONS\n", stepIndex);
		icmPrintBusConnections(bus[stepIndex]);
		icmPrintf("\n");

	}

}

////////////////////////////////////////////////////////////////////////////////
//                                  ARGUMENT PARSING
////////////////////////////////////////////////////////////////////////////////

//
// (Parse the argument list and set variables)
//
static void parseArgs(int argc, char **argv){
    // check for the application program name argument
    if((argc<MIN_ARGS) || (argc>MAX_ARGS)) {
        icmPrintf(
           "usage: %s %s\n\n",
            argv[0], usage
        );
        exit(1);
    }

    if (argc > NUMCORES_ARG) {
		icmPrintf("Input-NUMCORES = %s\n", argv[NUMCORES_ARG]);
        numcores = atoi(argv[NUMCORES_ARG]);
    }
	if (argc > STOPTIME_ARG) {
		icmPrintf("Input-STOPTIME = %s\n", argv[STOPTIME_ARG]);
        stoptime = (long double)atof(argv[STOPTIME_ARG]);
    }
}




////////////////////////////////////////////////////////////////////////////////
//                                   M A I N
////////////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[]) {

    parseArgs(argc, argv);

    // the constructor
    createPlatform();

	if(stoptime) {
    	icmSetSimulationStopTime((icmTime)stoptime);
	}

    // run till the end or stopTime
    icmSimulatePlatform();

    // finish and clean up
    icmTerminate();
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
