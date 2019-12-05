#ifndef __KERNEL_PIPE_H
#define __KERNEL_PIPE_H

#include "tinyos.h"
#include "kernel_dev.h"

#define BUF_SIZE 8000

typedef struct pipe_control_block
{				 								
	FCB *read,*write;							//periexi ta 2 fids read write
	char Buffer[BUF_SIZE];						//Bubbfer 4-16 kb
	int head,tail;								//2 pointers gia tin arki ke to telos tou buffer int i buffer point 
	CondVar cv_read,cv_write;					//2 Cond vars ena exo data ean exo xoro

	
}PIPECB;



void* pipe_open(uint m);
int no_write(void* this, const char* buf, unsigned int size);
int no_read(void* this, char *buf, unsigned int size);
int pipe_read(void* this, char *buf, unsigned int size);
int pipe_write(void* this, const char* buf, unsigned int size);
int reader_close(void* this);
int writer_close(void* this);


#endif