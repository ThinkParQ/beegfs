#ifndef INCOMINGDATAWORK_H_
#define INCOMINGDATAWORK_H_

#include <common/components/worker/Work.h>
#include <common/components/StreamListener.h>
#include <common/net/sock/Socket.h>


class IncomingDataWork : public Work
{
   public:
      /**
       * Note: Be aware that this is class is only for stream connections that need to be returned
       * to the streamListener after processing.
       */
      IncomingDataWork(StreamListener* streamListener, Socket* sock)
      {
         this->streamListener = streamListener;
         this->sock = sock;
      }

      virtual void process(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen);

      static void invalidateConnection(Socket* sock);
      static bool checkRDMASocketImmediateData(StreamListener* streamListener, Socket* sock);


   private:
      StreamListener* streamListener;
      Socket* sock;
};

#endif /*INCOMINGDATAWORK_H_*/
