

#ifndef ROSHTEST2_H_
#define ROSHTEST2_H_


#define MAX_BUFF_SIZE 32

#define WAIT_DELAY 1000


// stack implemented using buffer (LIFO)
struct LinearBuffer{
    char data[MAX_BUFF_SIZE];
    int size;	// size changes as you pop/push items from stack
} ;



void LinearBuffer_create(int len);
void LinearBuffer_destroy();
int LinearBuffer_insert(char c);
int LinearBuffer_remove();
void LinearBuffer_printcontents();
void LinearBuffer_dummyPop(char* str);

void TEST2_Producer(char c);
void TEST2_Consumer(void);

void TEST2_SharedMem(void);
void TEST2_VolatileBuff(void);


#endif /* ROSHTEST2_H_ */