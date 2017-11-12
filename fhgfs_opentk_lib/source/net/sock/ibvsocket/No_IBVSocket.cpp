#ifndef BEEGFS_OPENTK_IBVERBS

#include "IBVSocket.h"


#include <syslog.h>



void IBVSocket_init(IBVSocket* _this)
{
   _this->sockValid = false;
}

IBVSocket* IBVSocket_construct()
{
   IBVSocket* _this = (IBVSocket*)malloc(sizeof(*_this) );

   IBVSocket_init(_this);

   return _this;
}

void IBVSocket_uninit(IBVSocket* _this)
{
   // nothing to be done here
}

void IBVSocket_destruct(IBVSocket* _this)
{
   IBVSocket_uninit(_this);

   free(_this);
}

bool IBVSocket_rdmaDevicesExist(void)
{
   return false;
}

void IBVSocket_fork_init_once()
{
}

bool IBVSocket_connectByName(IBVSocket* _this, const char* hostname, unsigned short port,
   IBVCommConfig* commCfg)
{
   return false;
}

bool IBVSocket_connectByIP(IBVSocket* _this, struct in_addr* ipaddress, unsigned short port,
   IBVCommConfig* commCfg)
{
   return false;
}

bool IBVSocket_bind(IBVSocket* _this, unsigned short port)
{
   return false;
}

bool IBVSocket_bindToAddr(IBVSocket* _this, in_addr_t ipAddr, unsigned short port)
{
   return false;
}

bool IBVSocket_listen(IBVSocket* _this)
{
   return false;
}

IBVSocket_AcceptRes IBVSocket_accept(IBVSocket* _this, IBVSocket** outAcceptedSock,
   struct sockaddr* peerAddr, socklen_t *peerAddrLen)
{
   return ACCEPTRES_ERR;
}

bool IBVSocket_shutdown(IBVSocket* _this)
{
   return false;
}

ssize_t IBVSocket_recv(IBVSocket* _this, char* buf, size_t bufLen, int flags)
{
   return -1;
}

ssize_t IBVSocket_recvT(IBVSocket* _this, char* buf, size_t bufLen, int flags, int timeoutMS)
{
   return -1;
}

ssize_t IBVSocket_send(IBVSocket* _this, const char* buf, size_t bufLen, int flags)
{
   return -1;
}

/**
 * @return 0 on success, -1 on error
 */
int IBVSocket_checkConnection(IBVSocket* _this)
{
   return -1;
}

/**
 * @return <0 on error, 0 if recv would block, >0 if recv would not block
 */
ssize_t IBVSocket_nonblockingRecvCheck(IBVSocket* _this)
{
   return -1;
}

bool IBVSocket_checkDelayedEvents(IBVSocket* _this)
{
   return false;
}

bool IBVSocket_getSockValid(IBVSocket* _this)
{
   return _this->sockValid;
}

int IBVSocket_getRecvCompletionFD(IBVSocket* _this)
{
   return -1;
}

int IBVSocket_getConnManagerFD(IBVSocket* _this)
{
   return -1;
}

void IBVSocket_setTypeOfService(IBVSocket* _this, uint8_t typeOfService)
{
}

#endif // BEEGFS_OPENTK_IBVERBS

