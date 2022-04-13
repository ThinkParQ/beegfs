#ifndef DATAGRAMLISTENER_H_
#define DATAGRAMLISTENER_H_

#include <app/log/Logger.h>
#include <common/threading/Thread.h>
#include <common/net/sock/StandardSocket.h>
#include <common/net/message/NetMessage.h>
#include <nodes/NodeStoreEx.h>
#include <common/toolkit/NetFilter.h>
#include <common/Common.h>


#define DGRAMMGR_RECVBUF_SIZE    65536
#define DGRAMMGR_SENDBUF_SIZE    DGRAMMGR_RECVBUF_SIZE


struct DatagramListener;
typedef struct DatagramListener DatagramListener;

static inline __must_check bool DatagramListener_init(DatagramListener* this, App* app,
   Node* localNode, unsigned short udpPort);
static inline DatagramListener* DatagramListener_construct(App* app, Node* localNode,
   unsigned short udpPort);
static inline void DatagramListener_uninit(DatagramListener* this);
static inline void DatagramListener_destruct(DatagramListener* this);

//extern ssize_t DatagramListener_broadcast(DatagramListener* this, void* buf, size_t len,
//   int flags, unsigned short port); // no longer needed
extern void DatagramListener_sendMsgToNode(DatagramListener* this, Node* node, NetMessage* msg);

extern void __DatagramListener_run(Thread* this);
extern void __DatagramListener_listenLoop(DatagramListener* this);

extern void _DatagramListener_handleIncomingMsg(DatagramListener* this,
   fhgfs_sockaddr_in* fromAddr, NetMessage* msg);

extern bool __DatagramListener_initSock(DatagramListener* this, unsigned short udpPort);
extern void __DatagramListener_initBuffers(DatagramListener* this);
extern bool __DatagramListener_isDGramFromLocalhost(DatagramListener* this,
   fhgfs_sockaddr_in* fromAddr);


struct DatagramListener
{
   Thread thread;

   App* app;

   char* recvBuf;

   Node* localNode;
   NetFilter* netFilter;

   StandardSocket* udpSock;
   unsigned short udpPortNetByteOrder;
   char* sendBuf;
   Mutex sendMutex;
};


bool DatagramListener_init(DatagramListener* this, App* app, Node* localNode,
   unsigned short udpPort)
{
   Thread_init( (Thread*)this, BEEGFS_THREAD_NAME_PREFIX_STR "DGramLis", __DatagramListener_run);

   this->app = app;
   this->udpSock = NULL;

   this->localNode = localNode;
   this->netFilter = App_getNetFilter(app);

   this->recvBuf = NULL;
   this->sendBuf = NULL;

   Mutex_init(&this->sendMutex);

   if(!__DatagramListener_initSock(this, udpPort) )
   {
      Logger* log = App_getLogger(app);
      const char* logContext = "DatagramListener_init";

      Logger_logErr(log, logContext, "Unable to initialize the socket");
      goto err;
   }

   return true;

err:
   Mutex_uninit(&this->sendMutex);
   return false;
}

struct DatagramListener* DatagramListener_construct(App* app, Node* localNode,
   unsigned short udpPort)
{
   struct DatagramListener* this = kmalloc(sizeof(*this), GFP_NOFS);

   if(!this ||
      !DatagramListener_init(this, app, localNode, udpPort) )
   {
      kfree(this);
      return NULL;
   }

   return this;
}

void DatagramListener_uninit(DatagramListener* this)
{
   Socket* udpSockBase = (Socket*)this->udpSock;

   if(udpSockBase)
      Socket_virtualDestruct(udpSockBase);

   Mutex_uninit(&this->sendMutex);

   SAFE_VFREE(this->sendBuf);
   SAFE_VFREE(this->recvBuf);

   Thread_uninit( (Thread*)this);
}

void DatagramListener_destruct(DatagramListener* this)
{
   DatagramListener_uninit(this);

   kfree(this);
}


static inline ssize_t DatagramListener_sendto_kernel(DatagramListener* this, void* buf, size_t len, int flags,
   fhgfs_sockaddr_in* to)
{
   ssize_t sendRes;

   Mutex_lock(&this->sendMutex);

   sendRes = Socket_sendto_kernel(&this->udpSock->pooledSocket.socket, buf, len, flags, to);

   Mutex_unlock(&this->sendMutex);

   return sendRes;
}

static inline ssize_t DatagramListener_sendtoIP_kernel(DatagramListener* this, void *buf, size_t len, int flags,
   struct in_addr ipAddr, unsigned short port)
{
   fhgfs_sockaddr_in peer = {
      .addr = ipAddr,
      .port = htons(port),
   };

   return DatagramListener_sendto_kernel(this, buf, len, flags, &peer);
}

#endif /*DATAGRAMLISTENER_H_*/
