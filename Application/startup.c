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

/*
 * Simple startup code for the Cortex-M3
 */

void Reset(void);
static void Nmi(void);
void Fault(void);
extern void xPortPendSVHandler(void);
extern void xPortSysTickHandler(void);
extern void vPortSVCHandler( void );

// Reserve space for the system stack.
#ifndef STACK_SIZE
#define STACK_SIZE                              1048576
#endif
static unsigned long pulMainStack[STACK_SIZE];

// The minimal vector table for a Cortex-M3.
__attribute__ ((section("vectors")))
void (* const nVectors[])(void) =
{
    (void (*)(void))((unsigned long)pulMainStack + sizeof(pulMainStack)),
    Reset,
    Nmi,
    Fault,								//FAULT
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    vPortSVCHandler,						// SVCall handler
    0,
    0,
    xPortPendSVHandler,                     // The PendSV handler
    xPortSysTickHandler,                    // The SysTick handler
    0,
    0,
    0,
    0,
    0,
    0
};

// The following are constructs created by the linker, indicating where the
// the "data" and "bss" segments reside in memory.  The initializers for the
// for the "data" segment resides immediately following the "text" segment.
extern unsigned long _etext;
extern unsigned long _data;
extern unsigned long _edata;
extern unsigned long _bss;
extern unsigned long _ebss;

void Reset(void) {
    //
    // Call the application's entry point.
    //
    Main();
}

static void Nmi(void) {
    // infinite loop.
    while(1) {
    }
}

void Fault(void) {
    // infinite loop.
    while(1) {
    }
}
