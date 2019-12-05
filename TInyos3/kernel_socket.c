
#include "tinyos.h"
#include "kernel_socket.h"
#include "kernel_dev.h"
#include "kernel_sched.h"
#include "kernel_streams.h"
#include "kernel_proc.h"
#include "kernel_pipe.h"
#include "kernel_cc.h"
#include "util.h"



SCB* PortT[MAX_PORT]={NULL};//initialize table


void* socket_open(uint m)
{
  return NULL;
}

int socket_read(void* this, char *buf, unsigned int size)//a socket reads data from the receiver pipe
{
  SCB* scb = (SCB*)this;
  return pipe_read(scb->ps->pipe_receive, buf, size);
  
}
int socket_write(void* this, const char* buf, unsigned int size)
{
  SCB* scb = (SCB*)this;
  return pipe_write(scb->ps->pipe_send, buf, size); 
}

int socket_close(void* this)
{
  SCB* scb = (SCB*)this;
  
  
  if (scb->stype==LISTENER)  //the socket was a listener
  {
    Cond_Broadcast(&(scb->socket_cv));//wake up all the requests that couldn't connect 
    free(scb->ls);                    //free the listener data
  }
 else if (scb->stype==PEER ) 
  {
    scb->ps->pipe_receive->read=NULL;
    scb->ps->pipe_send->write=NULL;
  }
  scb->scb_fcb=NULL;
  free(scb);      //free the socket
  return 0;
}


file_ops  socket_fops = {
  .Open = socket_open,
  .Read = socket_read,
  .Write = socket_write,
  .Close = socket_close
};


Fid_t Socket(port_t port)//create a socket and bound it to a port (not if NOPORT)
{
  Fid_t fid;
  FCB* fcb;
  SCB *scb;
  if (port>MAX_PORT || port<0)    //check that 0<=port<=15
  	goto finerr;

  if(!FCB_reserve(1, &fid, &fcb)) //try to reserve a free FCB
    goto finerr;

  scb=(SCB*)xmalloc(sizeof(SCB));  //allocate memory for the socket
  scb->sfid=fid;                   //initialize its variables
  scb->stype=UNBOUND;              //
  scb->scb_fcb=fcb;                //
  scb->refcount=0;                 //
  scb->sport=port;                 //   
  scb->socket_cv=COND_INIT;        //
  fcb->streamobj=scb;              //stream object is socket control block
  fcb->streamfunc=&socket_fops;    //fops of the socket

  goto finok;

  finerr://ERROR, must return NOFILE
  fid = NOFILE;
  finok: //SUCCESS, return the new fid
  return fid;
}

int Listen(Fid_t sock) //Create a Listener from an Unbound socket
{

  SCB *scb;
  FCB* sfcb=get_fcb(sock);
  
  if (sfcb==NULL || sfcb->streamfunc!=&socket_fops) //check for valid FCB
 	goto finerr;
 	
  scb=(SCB*)sfcb->streamobj;
  
  if (scb->sport==NOPORT || scb->stype==LISTENER ) //check for UNBOUND socket type
  	goto finerr;
  if(PortT[scb->sport]!=NULL && PortT[scb->sport]->stype==LISTENER)//check for preexisting Listener at that port
 	goto finerr;

  //all checks have been passed, create a Listener
  scb->ls=(LS*)xmalloc(sizeof(LS));
  scb->stype=LISTENER;
  scb->ls->listenerflag=0; 
  rlnode_init(& scb->ls->lscb_list, NULL);
  PortT[scb->sport]=scb; //the port points to the listener
 
  goto finok;
  
  finerr://ERROR, must return -1
  return -1;
  
  finok: //SUCCESS, must return 0
  return 0;
}

Fid_t Accept(Fid_t lsock) //the listener accepts a request and establishes a connection
{
  /*
			    /(read)----[[pipe1]]---(write)\
	-------/              							   \-------
  |PEER1 | 		    			          		   | PEER2|
	-------\				    			             /-------
			    \(write)---[[pipe2]]----(read)/
  */
  
  FCB* fcb = get_fcb(lsock); // fcb of the listener 
  SCB *scb,*peer1,*peer2;
  RCB *rcb=NULL;
  rlnode *tmp;

  FCB *fcb2;  //accept fcb
  Fid_t fid2; //accept fid

  PIPECB *p1,*p2;
  if (fcb==NULL || fcb->streamfunc!=&socket_fops )//check if fid is in limits and it is an scb
 	goto finerr;
  scb=(SCB*)fcb->streamobj;   
  
  if (scb->stype!=LISTENER) //check if it is a listener 
 	goto finerr;

  while(is_rlist_empty(&(scb->ls->lscb_list)))	//if the request rist is empty the listener will sleep
  {
    Mutex_Lock(&kernel_mutex);
    Cond_Wait(&kernel_mutex,&(scb->socket_cv));
    Mutex_Unlock(&kernel_mutex);
  }

  // allocating memory for two pipes
  p1 = (PIPECB*)xmalloc(sizeof(PIPECB));
  p2 = (PIPECB*)xmalloc(sizeof(PIPECB));

  p1->head=0;
  p1->tail=0;
  p1->cv_read=COND_INIT;
  p1->cv_write=COND_INIT;
  
  p2->head=0;
  p2->tail=0;
  p2->cv_read=COND_INIT;
  p2->cv_write=COND_INIT;
 
  tmp=rlist_pop_front(&(scb->ls->lscb_list));//pop the first request
  rcb=tmp->rcb;
  peer1=rcb->scb;//the socket that made the request

  // the socket becomes a PEER and points to 2 pipes
  peer1->stype=PEER;
  peer1->ps=(PS*)xmalloc(sizeof(PS));
  peer1->ps->pipe_receive=p1;
  peer1->ps->pipe_send=p2;
    
  // create a new PEER for P2P connection
  fid2=Socket(scb->sport);
  if (fid2==NOFILE)   //na to kano pio pano na min kani pop ke na xenete to to stixio...
   goto finerr;
   
  fcb2=get_fcb(fid2);
  peer2=(SCB*)fcb2->streamobj;
  peer2->stype=PEER;  
  peer2->ps=(PS*)xmalloc(sizeof(PS));
  peer2->ps->pipe_receive=p2; //peer1 uses it as pipe_send
  peer2->ps->pipe_send=p1;    //peer1 uses it as pipe_receive
  
  
  p1->read=peer1->scb_fcb;
  p1->write=peer2->scb_fcb;
  
  p2->read=peer2->scb_fcb;
  p2->write=peer1->scb_fcb;
 

  goto finok;

  finerr: //ERROR
  if(rcb!=NULL)
  {
    rcb->succes_flag = 2;//error flag
    Cond_Signal(&(peer1->socket_cv));
  }
    
  return -1;

  finok: //SUCCESS, wake up the PEER (previously UNBOUND)
  rcb->succes_flag = 1; //success flag
  Cond_Signal(&(peer1->socket_cv)); 
   
  return fid2;
}


int Connect(Fid_t sock, port_t port, timeout_t timeout) //an UNBOUND socket requests to connect to a port
{
  SCB *scb1;
  RCB *rcb;
  FCB* sfcb=get_fcb(sock);
  
  
  if (port<=NOPORT || port >MAX_PORT)                   //check for valid port
    goto finerr;
  if (sfcb==NULL || sfcb->streamfunc!=&socket_fops )    //check for valid FCB
    goto finerr;
  if(PortT[port]==NULL || PortT[port]->stype!=LISTENER) //check for Listener at that port
    goto finerr;
  scb1=(SCB*)sfcb->streamobj;
  if (scb1->stype==PEER || scb1->stype==LISTENER )      //check that UNBOUND socket wants to connect
    goto finerr;
  // starting the connection
  scb1->sport = port;

  // initialize the request control block and push it back at the listener's request queue
  rcb=(RCB*)xmalloc(sizeof(RCB));
  rcb->succes_flag=0;
  rcb->scb=scb1;
  rlnode_init(& rcb->request_node, rcb);
  rlist_push_back(&(PortT[port]->ls->lscb_list),&(rcb->request_node));
  while(rcb->succes_flag==0) 
  {                       //no connection yet
  	Cond_Signal(&(PortT[port]->socket_cv));          //the socket wakes the listener
    Mutex_Lock(&kernel_mutex);
    Cond_Wait(&kernel_mutex,&(rcb->scb->socket_cv)); //and sleeps until the listener wakes it up
    Mutex_Unlock(&kernel_mutex);
  }
  if(rcb->succes_flag==2)
    goto finerr;

  free(rcb); //connected, free the request

  goto finok;  
  finerr://ERROR
  return -1;
  
  finok: //SUCCESS
  return 0;
}


int ShutDown(Fid_t sock, shutdown_mode how)
{	

  SCB *scb;
  FCB* sfcb=get_fcb(sock);
  if (sfcb==NULL || sfcb->streamfunc!=&socket_fops)   //check for valid FCB
    goto finerr;

  scb=(SCB*)sfcb->streamobj;
  if (scb->stype==LISTENER || scb->stype==UNBOUND)    //check that socket is PEER
    goto finerr;

  switch (how)
  {
    case 1:
      reader_close(scb->ps->pipe_receive);
      //scb->ps->pipe_receive->read=NULL;
      break;
    case 2:
      writer_close(scb->ps->pipe_send);
      //scb->ps->pipe_send->write=NULL;
      break;
    case 3:
      reader_close(scb->ps->pipe_receive);
      writer_close(scb->ps->pipe_send);
      //scb->ps->pipe_receive->read=NULL;
      //scb->ps->pipe_send->write=NULL;
      break;
    default:
      goto finerr;
  }

  goto finok;
  finerr:
   return -1;
  finok:
   return 0;
}

