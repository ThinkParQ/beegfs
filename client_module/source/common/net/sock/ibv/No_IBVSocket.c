#include "IBVSocket.h"

#ifndef BEEGFS_RDMA

bool IBVSocket_init(IBVSocket* _this)
{
   printk_fhgfs(KERN_INFO, "%s:%d: You should never see this message\n", __func__, __LINE__);
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


bool IBVSocket_connectByIP(IBVSocket* _this, struct in_addr* ipaddress, unsigned short port,
   IBVCommConfig* commCfg)
{
   printk_fhgfs(KERN_INFO, "%s:%d: You should never see this message\n", __func__, __LINE__);
   return false;
}


bool IBVSocket_bindToAddr(IBVSocket* _this, struct in_addr* ipAddr, unsigned short port)
{
   printk_fhgfs(KERN_INFO, "%s:%d: You should never see this message\n", __func__, __LINE__);
   return false;
}


bool IBVSocket_listen(IBVSocket* _this)
{
   printk_fhgfs(KERN_INFO, "%s:%d: You should never see this message\n", __func__, __LINE__);
   return false;
}


bool IBVSocket_shutdown(IBVSocket* _this)
{
   printk_fhgfs(KERN_INFO, "%s:%d: You should never see this message\n", __func__, __LINE__);
   return false;
}


ssize_t IBVSocket_recvT(IBVSocket* _this, struct iov_iter* iter, int flags, int timeoutMS)
{
   printk_fhgfs(KERN_INFO, "%s:%d: You should never see this message\n", __func__, __LINE__);
   return -1;
}


ssize_t IBVSocket_send(IBVSocket* _this, struct iov_iter* iter, int flags)
{
   printk_fhgfs(KERN_INFO, "%s:%d: You should never see this message\n", __func__, __LINE__);
   return -1;
}


/**
 * @return 0 on success, -1 on error
 */
int IBVSocket_checkConnection(IBVSocket* _this)
{
   printk_fhgfs(KERN_INFO, "%s:%d: You should never see this message\n", __func__, __LINE__);
   return -1;
}


unsigned long IBVSocket_poll(IBVSocket* _this, short events, bool finishPoll)
{
   printk_fhgfs(KERN_INFO, "%s:%d: You should never see this message\n", __func__, __LINE__);
   return ~0;
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

#endif

