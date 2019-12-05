#ifndef __KERNEL_THREADS_H
#define __KERNEL_THREADS_H


#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_proc.h"

typedef enum ptcbid_state_e {
  FREEPTCB,   /**< The PID is free and available */
  ALIVEPTCB,  /**< The PID is given to a process */
  ZOMBIEPTCB  /**< The PID is held by a zombie */
} ptcbid_state;

typedef struct process_thread_control_block
{
  ptcbid_state  ptcbstate;

  Task task;         /**< The thread's function apla 8a to pername  */
  TCB* thread;       /**< The TCB thread in the ptcb*/

  int detached;
  int exitval;
  CondVar cv; 

  Tid_t tid;
  int argl;               /**< The main thread's argument length */
  void* args;           /**< The main thread's argument string */
  
  rlnode ptcb_node;

  struct process_thread_control_block * prev; 
  struct process_thread_control_block * next; 
  
}PTCB;



void initialize_ptcbs();

PTCB* acquire_PTCB();

#endif
