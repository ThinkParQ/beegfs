#ifndef CACHECONNWORKER_H_
#define CACHECONNWORKER_H_

#include <common/components/ComponentInitException.h>
#include <common/components/worker/UnixConnWorker.h>
#include <common/net/sock/NamedSocket.h>
#include <common/threading/PThread.h>



/**
 * Handles connections from the local machine to itself (to avoid deadlocks by using
 * the standard work-queue mechanism).
 */
class CacheConnWorker : public UnixConnWorker
{
   public:
      /**
       * Constructor for named socket version.
       */
      CacheConnWorker(const std::string& workerID, const std::string& namedSocketPath);

      virtual ~CacheConnWorker();


   private:
      char* bufIn;
      char* bufOut;

      NamedSocket* socket;


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
         return socket;
      }
};

#endif /*CACHECONNWORKER_H_*/
