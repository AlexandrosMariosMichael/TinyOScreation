#ifndef __KERNEL_SOCKET_H
#define __KERNEL_SOCKET_H

#include "tinyos.h"
#include "kernel_streams.h"
#include "kernel_pipe.h"
#include "util.h"



typedef enum socket_types{
  UNBOUND,  
  LISTENER,  
  PEER
}socket_type;


typedef struct listener_struct
{
  int listenerflag;
  rlnode lscb_list;
	
}LS;
typedef struct peer_struct
{
	PIPECB *pipe_receive, *pipe_send;
}PS;


typedef struct socket_control_block
{
  socket_type stype;
  port_t sport;
  Fid_t sfid;
  FCB *scb_fcb;
  CondVar socket_cv;
  union
  {
  	LS* ls;
  	PS* ps;
  };
  int refcount;

}SCB;

typedef struct request_control_block 
{
  rlnode request_node;
  SCB *scb;
  int succes_flag; 	//Flag for successful connection
  
}RCB;

#endif