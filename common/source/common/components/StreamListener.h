#ifndef STREAMLISTENER_H_
#define STREAMLISTENER_H_

#include <common/app/log/LogContext.h>
#include <common/components/worker/queue/MultiWorkQueue.h>
#include <common/components/ComponentInitException.h>
#include <common/net/sock/StandardSocket.h>
#include <common/net/sock/RDMASocket.h>
#include <common/net/message/NetMessage.h>
#include <common/nodes/Node.h>
#include <common/threading/PThread.h>
#include <common/toolkit/poll/PollList.h>
#include <common/toolkit/Pipe.h>
#include <common/Common.h>


class StreamListener : public PThread
{
   public:
      StreamListener(NicAddressList& localNicList, MultiWorkQueue* workQueue,
         unsigned short listenPort);
      virtual ~StreamListener();


   private:
      LogContext        log;
      StandardSocket*   tcpListenSock;
      StandardSocket*   sdpListenSock;
      RDMASocket*       rdmaListenSock;

      MultiWorkQueue*   workQueue;

      int               epollFD;
      PollList          pollList;
      Pipe*             sockReturnPipe;

      Time              rdmaCheckT;
      int               rdmaCheckForceCounter;

      bool initSockReturnPipe();
      bool initSocks(unsigned short listenPort, NicListCapabilities* localNicCaps);

      virtual void run();
      void listenLoop();

      void onIncomingStandardConnection(StandardSocket* sock);
      void onIncomingRDMAConnection(RDMASocket* sock);
      void onIncomingData(Socket* sock);
      void onSockReturn();
      void rdmaConnIdleCheck();

      void applySocketOptions(StandardSocket* sock);
      bool isFalseAlarm(RDMASocket* sock);

      void deleteAllConns();


   public:
      // getters & setters
      FileDescriptor* getSockReturnFD()
      {
         return sockReturnPipe->getWriteFD();
      }

      MultiWorkQueue* getWorkQueue() const
      {
         return this->workQueue;
      }

};

#endif /*STREAMLISTENER_H_*/
