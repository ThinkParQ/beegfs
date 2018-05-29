#ifndef INCOMINGPREPROCESSEDMSGWORK_H_
#define INCOMINGPREPROCESSEDMSGWORK_H_

#include <common/app/AbstractApp.h>
#include <common/components/worker/Work.h>
#include <common/net/message/NetMessage.h>
#include <common/net/sock/Socket.h>
#include <common/Common.h>


class IncomingPreprocessedMsgWork : public Work
{
   public:
      /**
       * Note: Be aware that this class is only for stream connections that need to be returned
       * to a StreamListenerV2 after processing.
       *
       * @param msgHeader contents will be copied
       */
      IncomingPreprocessedMsgWork(AbstractApp* app, Socket* sock, NetMessageHeader* msgHeader)
      {
         this->app = app;
         this->sock = sock;
         this->msgHeader = *msgHeader;
      }

      virtual void process(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen);

      static void releaseSocket(AbstractApp* app, Socket** sock, NetMessage* msg);
      static void invalidateConnection(Socket* sock);
      static bool checkRDMASocketImmediateData(AbstractApp* app, Socket* sock);


   private:
      AbstractApp* app;
      Socket* sock;
      NetMessageHeader msgHeader;
};

#endif /*INCOMINGPREPROCESSEDMSGWORK_H_*/
