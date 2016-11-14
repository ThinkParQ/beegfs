#ifndef STORAGEBENCHWORK_H_
#define STORAGEBENCHWORK_H_

#include <common/benchmark/StorageBench.h>
#include <common/components/worker/Work.h>
#include <common/toolkit/Pipe.h>
#include <common/Common.h>



class StorageBenchWork: public Work
{
   public:
      StorageBenchWork(uint16_t targetID, int threadID, int fileDescriptor,
         StorageBenchType type, int64_t bufLen, Pipe* operatorCommunication, char* buf)
      {
         this->targetID = targetID;
         this->threadID = threadID;
         this->fileDescriptor = fileDescriptor;

         this->type = type;
         this->bufLen = bufLen;
         this->operatorCommunication = operatorCommunication;
         this->buf = buf;
      }

      virtual ~StorageBenchWork()
      {
      }

      void process(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen);

   protected:

   private:
      uint16_t targetID;
      int threadID; // virtual threadID
      int fileDescriptor;
      StorageBenchType type;
      int64_t bufLen;
      char* buf;
      Pipe* operatorCommunication;
};

#endif /* STORAGEBENCHWORK_H_ */
