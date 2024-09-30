#include "IBVSocket.h"

#ifndef BEEGFS_RDMA

#define no_ibvsocket_err() \
   printk_fhgfs(KERN_INFO, "%s:%d: You should never see this message\n", __func__, __LINE__)

bool IBVSocket_init(IBVSocket* _this, struct in_addr srcIpAddr, NicAddressStats* nicStats)
{
   no_ibvsocket_err();
   return false;
}

void IBVSocket_uninit(IBVSocket* _this)
{
   // nothing to be done here
}

bool IBVSocket_rdmaDevicesExist(void)
{
   return false;
}

bool IBVSocket_connectByIP(IBVSocket* _this, struct in_addr ipaddress, unsigned short port,
   IBVCommConfig* commCfg)
{
   no_ibvsocket_err();
   return false;
}

bool IBVSocket_bindToAddr(IBVSocket* _this, struct in_addr ipAddr, unsigned short port)
{
   no_ibvsocket_err();
   return false;
}

bool IBVSocket_listen(IBVSocket* _this)
{
   no_ibvsocket_err();
   return false;
}

bool IBVSocket_shutdown(IBVSocket* _this)
{
   no_ibvsocket_err();
   return false;
}

ssize_t IBVSocket_recvT(IBVSocket* _this, struct iov_iter* iter, int flags, int timeoutMS)
{
   no_ibvsocket_err();
   return -1;
}

ssize_t IBVSocket_send(IBVSocket* _this, struct iov_iter* iter, int flags)
{
   no_ibvsocket_err();
   return -1;
}

/**
 * @return 0 on success, -1 on error
 */
int IBVSocket_checkConnection(IBVSocket* _this)
{
   no_ibvsocket_err();
   return -1;
}

unsigned long IBVSocket_poll(IBVSocket* _this, short events, bool finishPoll)
{
   no_ibvsocket_err();
   return ~0;
}

unsigned IBVSocket_getRkey(IBVSocket* _this)
{
   no_ibvsocket_err();
   return ~0;
}

struct ib_device* IBVSocket_getDevice(IBVSocket* _this)
{
   return NULL;
}

void IBVSocket_setTimeouts(IBVSocket* _this, int connectMS,
   int completionMS, int flowSendMS, int flowRecvMS, int pollMS)
{
}

void IBVSocket_setTypeOfService(IBVSocket* _this, int typeOfService)
{
}

void IBVSocket_setConnectionFailureStatus(IBVSocket* _this, unsigned value)
{
}

struct in_addr IBVSocket_getSrcIpAddr(IBVSocket* _this)
{
   struct in_addr r = {
      .s_addr = ~0
   };
   return r;
}

NicAddressStats* IBVSocket_getNicStats(IBVSocket* _this)
{
   return NULL;
}

#endif

