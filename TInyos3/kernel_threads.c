
#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_proc.h"
#include "kernel_threads.h"
#include "kernel_cc.h"

PTCB  *newptcb;
PTCB PTT[MAX_PROC];
unsigned int ptcb_count;
static PTCB* ptcb_freelist;

Tid_t get_tid()
{
  return newptcb==NULL ? NOTHREAD : (newptcb+1)-PTT;
}

/* Initialize a PCB */
static inline void initialize_PTCB(PTCB* ptcb)
{
  ptcb->ptcbstate = FREEPTCB;
  ptcb->tid=NOTHREAD;
  ptcb->thread = NULL;
  ptcb->argl = 0;
  ptcb->args = NULL;
  ptcb->detached=0;
  ptcb->cv= COND_INIT;  
  rlnode_init(& ptcb->ptcb_node,ptcb);

}

void initialize_ptcbs()
{
  /* initialize the PTCBs */
  for(Tid_t p=0; p<MAX_PROC; p++) {
    initialize_PTCB(&PTT[p]);
  }

  PTCB* ptcbiter;
  ptcb_freelist = NULL;
  for(ptcbiter = PTT+MAX_PROC; ptcbiter!=PTT; ) {
    --ptcbiter;
    ptcbiter->prev = ptcb_freelist;
    ptcb_freelist = ptcbiter;
  }

  ptcb_count = 0;

}

PTCB* acquire_PTCB()
{
  PTCB* ptcb = NULL;

  if(ptcb_freelist != NULL)
  {
    ptcb = ptcb_freelist;
    ptcb->ptcbstate = ALIVEPTCB;
    ptcb_freelist = ptcb_freelist->prev;
    ptcb_count++;
  }

  return ptcb;
}

void release_PTCB(PTCB* ptcb) 
{
  if (ptcb->prev!=NULL )
    ptcb->prev->next = ptcb->next;
  ptcb->prev=ptcb_freelist;
  ptcb->prev->ptcbstate=FREEPTCB;
  ptcb_freelist=ptcb;
  ptcb_count--;

}

void start_other_thread()
{
  int exitval;

  
    Task call = CURTHREAD->owner_ptcb->task;
    int argl = CURTHREAD->owner_ptcb->argl;
    void* args = CURTHREAD->owner_ptcb->args;
    exitval = call(argl,args);
    ThreadExit(exitval);
}


/** 
  @brief Create a new thread in the current process.
  */
Tid_t CreateThread(Task task, int argl, void* args)
{
  PCB *proc;
  Mutex_Lock(&kernel_mutex);

  /*The new process PCB */
  newptcb = acquire_PTCB();//PTCB count is increased here
  proc = CURPROC;

  if(newptcb == NULL) goto finish;  /* We have run out of PTCB_IDs! */

  rlist_push_front(&proc->ptcb_list,&newptcb->ptcb_node);

  /* Set the thread's function */
  newptcb->task = task;

  /* Copy the arguments to new storage, owned by the new process */
  newptcb->argl = argl;
  if(args!=NULL)
    newptcb->args = args;
  else
    newptcb->args=NULL;

  if(task != NULL) {
    newptcb->thread = spawn_thread(proc,start_other_thread); //start_last_thread
    newptcb->thread->owner_ptcb = newptcb;
    wakeup(newptcb->thread);
  }

  newptcb->tid=get_tid();

 finish:
  Mutex_Unlock(&kernel_mutex);
return get_tid();
 
}

/**
  @brief Return the Tid of the current thread.
 */
Tid_t ThreadSelf()
{
	return (Tid_t) CURTHREAD;
}


/**
  @brief Join the given thread.
  */

int ThreadJoin(Tid_t tidd, int* exitval)
{
  PCB *proc;
   //The tid must refer to a legal thread
  // owned by the same process that owns the caller.
  proc=CURPROC;

  // Legality checks 
  if((tidd<0) || (tidd>=MAX_PROC) ) 
  {
    tidd=NOTHREAD;
    goto finish;
  }

  //Goes through list 
  rlnode* tmp= proc->ptcb_list.next;
  while(tmp->ptcb->tid!=tidd)
  {
    tmp=tmp->next;
  }

  //A thread cannot join itself or a detached thread
  if(tmp->ptcb->detached==1 || tmp->ptcb->tid == ThreadSelf())
  {
    tidd = NOTHREAD;
    goto finish;
  }

  while(tmp->ptcb->ptcbstate == ALIVEPTCB)
  {
    Mutex_Lock(&kernel_mutex);
    Cond_Wait(& kernel_mutex,&(tmp->ptcb->cv)); 
    Mutex_Unlock(& kernel_mutex);

  }

  rlist_remove(&(tmp->ptcb->ptcb_node));
  release_PTCB(tmp->ptcb);

  finish:
	return tidd==NOTHREAD ? -1 : 0;
}

/**
  @brief Detach the given thread.
  */
int ThreadDetach(Tid_t tidd)
{
  PCB *proc=CURPROC;
  
  // Legality checks 
  if((tidd<0) || (tidd>=MAX_PROC)) 
  {
    tidd = NOTHREAD;
    goto finish;
  }

  rlnode* tmp= proc->ptcb_list.next;
  while(tmp->ptcb->tid!=tidd)
  {
    tmp=tmp->next;
  }

  tmp->ptcb->detached=1;
  Cond_Broadcast(&(tmp->ptcb->cv));

  finish:
 	return tidd==NOTHREAD ? -1 : 0;
}

/**
  @brief Terminate the current thread.
  */

void ThreadExit(int exitval)
{
  // Now, we exit 
  PTCB *curptcb = CURTHREAD->owner_ptcb;

  //Do all the other cleanup we want here, close files etc. 
   if(curptcb->args) 
  {
    curptcb->args = NULL;
  }

  Cond_Broadcast(& curptcb->cv);

  // Bye-bye cruel world 
  curptcb->exitval = exitval;
  curptcb->ptcbstate = ZOMBIEPTCB;

  sleep_releasing(EXITED, & kernel_mutex);

 }


/**
  @brief Awaken the thread, if it is sleeping.

  This call will set the interrupt flag of the
  thread.

  */
int ThreadInterrupt(Tid_t tid)
{
	return -1;
}


/**
  @brief Return the interrupt flag of the 
  current thread.
  */
int ThreadIsInterrupted()
{
	return 0;
}

/**
  @brief Clear the interrupt flag of the
  current thread.
  */
void ThreadClearInterrupt()
{

}

