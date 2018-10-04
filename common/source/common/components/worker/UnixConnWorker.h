#ifndef COMMON_UNIXCONNWORKER_H_
#define COMMON_UNIXCONNWORKER_H_


#include <common/components/ComponentInitException.h>
#include <common/net/sock/StandardSocket.h>
#include <common/threading/PThread.h>



class UnixConnWorker : public PThread
{
   public:
      UnixConnWorker(const std::string& workerID, std::string threadName)
         : PThread(std::string(threadName) + workerID),
         log(std::string(threadName) + workerID),
         netMessageFactory(PThread::getCurrentThreadApp()->getNetMessageFactory() ),
         available(false) {};
      ~UnixConnWorker() {};

      virtual void run() = 0;
      virtual void workLoop() = 0;

      virtual bool processIncomingData(char* bufIn, unsigned bufInLen, char* bufOut,
         unsigned bufOutLen) = 0;
      virtual void invalidateConnection() = 0;

      virtual void applySocketOptions(StandardSocket* sock) = 0;
      virtual void initBuffers() = 0;

      virtual Socket* getClientEndpoint() = 0;


   protected:
      LogContext log;
      const AbstractNetMessageFactory* netMessageFactory;
      bool available; // == !acquired


   public:
      // getters & setters

      bool isAvailable()
      {
         return available;
      }

      void setAvailable(bool available)
      {
         this->available = available;
      }
};

#endif /* COMMON_UNIXCONNWORKER_H_ */
