/********************************************************************
Testing

- functions used for testing
*********************************************************************/

#include <stdio.h>

/* Scheduler include files. */
#include "FreeRTOS.h"

#include "bsp.h"

#include "simulatorIntercepts.h"

#include "roshTest2.h"

/**************************************************

Assume the following :
* SHARED MEM RANGE : 0x40005000 - 0x80104FFF	
* SIZE OF SHARED MEM : 0x400FFFFF		// 1024.999 MB (1GB)

* Common buffer area range : 0x4000500F	- 0x6000500F	// approx 512 MB
* Common synch/mutex flags : 0x6000501E - 0x80104FFF	// approx 512 MB

**************************************************/


char buff[32];

/*** Specify memory locations **/

// buffer

//struct LinearBuffer *Lbuffer = (struct LinearBuffer*) 0x4000500F; 
volatile struct LinearBuffer *Lbuffer = (volatile struct LinearBuffer*) 0x4000500F; 
// flags
volatile int *semaphore = (volatile int*) 0x6000501E;

/*** fuinction prototypes ***/
void lock();
void unlock();


/***********************
	HELPER functions 
***********************/

void prvWriteStrNoLF( const char *pcString )
{
    while( *pcString )
    {
//        vTaskDelay( CHAR_WRITE_DELAY );
        uartWrite( *pcString );
        pcString++;
    }
   
}




/***********************
	MUTEX Functions
************************/
void lock(){
	while(*semaphore==0){
		//prvWriteString("-- lock:: spining --");
		asm volatile("mov r0, r0");
	}
	*semaphore=0;
}

void unlock(){	
	*semaphore=1;
}


void wait(int delay){

	int i;
	for(i=0;i<delay;i++){
		asm volatile("mov r0, r0");
	}
}






/***********************
	Producer/Consumer
************************/
void TEST2_Producer(char c){
	sprintf(buff, "TEST2_Producer::Enter=%c", c);
	prvWriteString(buff);
	lock();
		while(Lbuffer->size == MAX_BUFF_SIZE){	// wait until buffer is not full
			unlock();
			prvWriteString("-- buff full:: spining --");
			//wait(1000);
			lock();
		}
		LinearBuffer_insert(c);
		LinearBuffer_printcontents();
	unlock();
	//wait(1000);
}


void TEST2_Consumer(){
	sprintf(buff, "TEST2_Consumer::Enter");
	prvWriteString(buff);
	lock();
		while(Lbuffer->size == 0){	// wait until buffer is not empty
			unlock();
			prvWriteString("-- buff empty:: spining --");
			//wait(100);
			lock();
		}
		LinearBuffer_remove();
		LinearBuffer_printcontents();
		
	unlock();
}


void TEST2_SharedMem(){
	
	int i;
	char string[] = "deadbeef";	// len = 8
	int cpuid = 123; 
	LinearBuffer_create(0); // initialise buffer
	cpuid = impProcessorId();
	
		switch(cpuid){

			// producer
			case 0 :
				for(i=0;i<8;i++){	// first 4 chars
					TEST2_Producer('0');
					//wait(1000);
				}
				break;

			// producer
			case 1 :				
				
				for(i=0;i<8;i++){	// last 4 chars
					TEST2_Producer('1');
				}
				
				
				break;

			// consumer
			case 2 :
				/*
				for(i=0;i<7;i++){
					TEST2_Consumer();
				}
				*/
				break;
				
			// consumer
			case 3 :
				
				/*
				for(i=3;i<7;i++){
					TEST2_Consumer();
				}
				*/
				break;

		}

}


void TEST2_VolatileBuff(){

	
	LinearBuffer_create(4); // initialise buffer
	LinearBuffer_dummyPop("hello");
	
	//sprintf(buff, "LinBuff.size=%d", Lbuffer->size);
	sprintf(buff, "LinBuff.data[0]=%s", Lbuffer->data);
	prvWriteString(buff);
	//LinearBuffer_printcontents();
}









/********************************
	LinearBuffer functions
*********************************/


// create - set size
void LinearBuffer_create(int len){
	Lbuffer->size = len;
	*semaphore=1;
}

// destroy - reset size
void LinearBuffer_destroy(){
	Lbuffer->size = 0;
}

// insert - add item to the top
int LinearBuffer_insert(char c){
	Lbuffer->data[Lbuffer->size++] = c;
	return Lbuffer->size;
}

// remove - remove item from the top
int LinearBuffer_remove(){
	Lbuffer->size--;
	return Lbuffer->size;
}

// print buffer contents
void LinearBuffer_printcontents(){
	int i,ix;
	//char tempbuff[256];
	//ix=0;
	for(i=0;i<Lbuffer->size;i++){
		
		sprintf(buff, "%c,",Lbuffer->data[i]);
		prvWriteStrNoLF(buff);
		
		//tempbuff[ix]=Lbuffer->data[i];
		//tempbuff[ix++]=',';
		//ix++;
	}
	prvWriteString(" ");
	//ix++;
	//tempbuff[ix]='\0';
		
	// print buffer contents
	//prvWriteString(tempbuff);
}


// populate buff with test string
void LinearBuffer_dummyPop(char* str){
	int i;
	for(i=0;i<4;i++){
		Lbuffer->data[i] = str[i];
	}
	Lbuffer->data[i+1]='\0';
}



/***********************/

/*
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
*/

