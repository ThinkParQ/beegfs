#ifndef RDMASOCKET_H_
#define RDMASOCKET_H_

#include <common/Common.h>
#include "PooledSocket.h"


class RDMASocket : public PooledSocket
{
   public:
      struct ImplCallbacks
      {
         bool (*rdma_devices_exist)();
         void (*rdma_fork_init_once)();
         RDMASocket* (*rdma_socket_create)();
      };

      static bool isRDMAAvailable();

      static std::unique_ptr<RDMASocket> create();

      static bool rdmaDevicesExist();
      static void rdmaForkInitOnce();

      virtual void checkConnection() = 0;
      virtual ssize_t nonblockingRecvCheck() = 0;
      virtual bool checkDelayedEvents() = 0;

      virtual void setBuffers(unsigned bufNum, unsigned bufSize) = 0;
      virtual void setTimeouts(int connectMS, int flowSendMS, int pollMS) = 0;
      virtual void setTypeOfService(uint8_t typeOfService) = 0;

      virtual void setConnectionRejectionRate(unsigned rate) = 0;
};


#endif /*RDMASOCKET_H_*/
