/********************************************************************
Testing

- functions used for testing
*********************************************************************/

#include <stdio.h>

/* Scheduler include files. */
#include "FreeRTOS.h"

/*
#include "task.h"
#include "queue.h"
#include "croutine.h"
*/

#include "bsp.h"

#include "simulatorIntercepts.h"

#include "roshTest.h"


/*** FOR TESTING **/
volatile int *flag = (volatile int*) 0x4000500F;
volatile int *count = (volatile int*) 0x400F5000;
/***********************/


void TEST_SharedMem(){
	
	char buff[32];
	int id = 123; 
	int i=0;
	
	
	*flag=123;
	*count=123;
	
	
	printf("helloworld\n");
	
	
	sprintf(buff, "The memory address of flag is: %p\n", flag);
	prvWriteString(buff);
	sprintf(buff, "The memory address of count is: %p\n", count);
	prvWriteString(buff);
	
	sprintf(buff, "The value of flag is: %d\n", *flag);
	prvWriteString(buff);
	sprintf(buff, "The value of count is: %d\n", *count);
	prvWriteString(buff);
	
	*flag=0;
	*count=0;
	
	id = impProcessorId();
	sprintf(buff,">> CORE: %d\n", id);
	//printf("%s\n",buff);
	//printf("hello world");
	prvWriteString(buff);
	
	
	prvWriteString("--going into conditional\n");
	if(id==1){
		sprintf(buff,"CORE-0: Count: %d\n", *count);
		prvWriteString(buff);
		while(*flag==0){prvWriteString("-- im waiting\n");} // wait until flag is true
				
		sprintf(buff, "The value of flag is: %d\n", *flag);
		prvWriteString(buff);
		sprintf(buff, "The value of count is: %d\n", *count);
		prvWriteString(buff);
		
		sprintf(buff,"CORE-0: Count: %d\n", *count);
		prvWriteString(buff);
	}
	else
	if(id==0){
		
		//for(i=0;i<100000;i++){asm("nop");}
		
		sprintf(buff,"CORE-1: Count: %d\n", *count);
		prvWriteString(buff);
		
		// increment count and set flag
		*count = 1;
		*flag = 1;
		
		sprintf(buff, "The value of flag is: %d\n", *flag);
		prvWriteString(buff);
		sprintf(buff, "The value of count is: %d\n", *count);
		prvWriteString(buff);		
		
		sprintf(buff,"CORE-1: Count: %d\n", *count);
		prvWriteString(buff);		
	}
	else{
		prvWriteString("--invalid\n");	
	}
}