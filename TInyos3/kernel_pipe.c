
#include "tinyos.h"
#include "kernel_dev.h"
#include "kernel_sched.h"
#include "kernel_streams.h"
#include "kernel_proc.h"
#include "kernel_pipe.h"
#include "kernel_threads.h"
#include "kernel_cc.h"


void* pipe_open(uint m)
{
  return NULL;
}

int no_write(void* this, const char* buf, unsigned int size)
{
  return NOFILE;
}

int no_read(void* this, char *buf, unsigned int size)
{
  return NOFILE;
}

int pipe_read(void* this, char *buf, unsigned int size)
{

  PIPECB* pipe_cb = (PIPECB*)this;
  unsigned int count=0;

  if (pipe_cb->read==NULL)
    return -1;
  if ((pipe_cb->write==NULL) && (((pipe_cb->tail)-(pipe_cb->head))== 0))
    return 0;
  while(count<size)  
  {
    while((pipe_cb->tail)-(pipe_cb->head)== 0) 
    {
      if(pipe_cb->write==NULL)
        return count;
      else
      {
        Cond_Signal(&(pipe_cb->cv_write));
        Mutex_Lock(&kernel_mutex);
        Cond_Wait(&kernel_mutex,&(pipe_cb->cv_read));
        Mutex_Unlock(&kernel_mutex);
      }
    }
	  buf[count]=pipe_cb->Buffer[pipe_cb->tail];
    count++;
    pipe_cb->tail=(pipe_cb->tail+1)%BUF_SIZE;
  }
  Cond_Signal(&(pipe_cb->cv_write));
  return count;
}

int pipe_write(void* this, const char* buf, unsigned int size)
{
  PIPECB* pipe_cb = (PIPECB*)this;
  unsigned int count=0;
  
  if( pipe_cb->read==NULL || pipe_cb->write==NULL)
    return -1;
  while(count < size) 
  {
    while((pipe_cb->tail)-(pipe_cb->head)== 1 || (pipe_cb->tail)-(pipe_cb->head) == 1-BUF_SIZE) 
    {
    	Cond_Signal(&(pipe_cb->cv_read));
      Mutex_Lock(&kernel_mutex);
      Cond_Wait(&kernel_mutex,&(pipe_cb->cv_write));
      Mutex_Unlock(&kernel_mutex);
    }
    pipe_cb->Buffer[pipe_cb->head]=buf[count];
    count++;
    pipe_cb->head=(pipe_cb->head+1)%BUF_SIZE;
  }
  Cond_Signal(&(pipe_cb->cv_read));
  return count;
}



int reader_close(void* this)
{
  PIPECB* pipe_cb = (PIPECB*)this;
  pipe_cb->read=NULL;
  if( pipe_cb->write==NULL)
     free(pipe_cb);

  return 0;
}

int writer_close(void* this)
{
  PIPECB* pipe_cb = (PIPECB*)this;
  pipe_cb->write=NULL;
  if(pipe_cb->read==NULL)
    free(pipe_cb);

  return 0;
}

file_ops  reader_fops = {
  .Open = pipe_open,
  .Read = pipe_read,
  .Write = no_write,
  .Close = reader_close
};

file_ops writer_fops = {
  .Open = pipe_open,
  .Read = no_read,
  .Write = pipe_write,
  .Close = writer_close
};

int Pipe(pipe_t* pipe)
{
  Fid_t fid[2];
  PIPECB *pipe_cb;
  FCB* fcb[2];

  Mutex_Lock(&kernel_mutex);
 
  if(FCB_reserve(2, fid, fcb)==0 )
      goto finerr;

 pipe_cb=(PIPECB*)xmalloc(sizeof(PIPECB));
 pipe_cb->read=fcb[1];
 pipe_cb->write=fcb[0];
 pipe->read=fid[1]; 
 pipe->write=fid[0]; 
 pipe_cb->head=0;
 pipe_cb->tail=0;
 pipe_cb->cv_read=COND_INIT;
 pipe_cb->cv_write=COND_INIT;

 fcb[1]->streamobj=pipe_cb;
 fcb[1]->streamfunc=&reader_fops;
 fcb[0]->streamobj=pipe_cb;
 fcb[0]->streamfunc=&writer_fops;
 
  goto finok;
finerr:
  Mutex_Unlock(&kernel_mutex);
  return -1;
finok:
  Mutex_Unlock(&kernel_mutex);
  return 0; 
  

}

