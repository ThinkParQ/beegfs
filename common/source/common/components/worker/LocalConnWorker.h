#ifndef LOCALCONNWORKER_H_
#define LOCALCONNWORKER_H_

#include <common/components/ComponentInitException.h>
#include <common/components/worker/UnixConnWorker.h>
#include <common/threading/PThread.h>
#include <common/net/sock/StandardSocket.h>


/**
 * Handles connections from the local machine to itself (to avoid deadlocks by using
 * the standard work-queue mechanism).
 */
class LocalConnWorker : public UnixConnWorker
{
   public:
      /**
       * Constructor for socket pair version.
       */
      LocalConnWorker(const std::string& workerID);

      virtual ~LocalConnWorker();


   private:
      char* bufIn;
      char* bufOut;

      StandardSocket* workerEndpoint;
      StandardSocket* clientEndpoint;

      virtual void run();
      void workLoop();

      bool processIncomingData(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen);
      void invalidateConnection();

      void applySocketOptions(StandardSocket* sock);
      void initBuffers();


   public:
      // getters & setters
      Socket* getClientEndpoint()
      {
         return clientEndpoint;
      }
};

#endif /*LOCALCONNWORKER_H_*/
