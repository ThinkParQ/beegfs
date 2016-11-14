#ifndef CLOSECHUNKFILEWORK_H_
#define CLOSECHUNKFILEWORK_H_

#include <common/components/worker/Work.h>
#include <common/net/sock/Socket.h>
#include <common/storage/StorageErrors.h>
#include <common/toolkit/SynchronizedCounter.h>
#include <common/storage/striping/StripeNodeFileInfo.h>
#include <common/Common.h>


class CloseChunkFileWork : public Work
{
   public:
      /**
       * @param outDynAttribs may be NULL if caller is not interested
       */
      CloseChunkFileWork(std::string& sessionID, std::string& fileHandleID, uint16_t targetID,
         DynamicFileAttribs *outDynAttribs, FhgfsOpsErr* outResult, SynchronizedCounter* counter) :
         sessionID(sessionID), fileHandleID(fileHandleID), targetID(targetID), isMirror(false),
         outDynAttribs(outDynAttribs), outResult(outResult), counter(counter)
      {
         // all assignments done in initializer list
      }

      virtual ~CloseChunkFileWork() {}


      virtual void process(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen);


   private:
      std::string sessionID;
      std::string fileHandleID;

      uint16_t targetID;
      bool isMirror; // true if this msg goes to a mirror of a chunk file
      DynamicFileAttribs* outDynAttribs;
      FhgfsOpsErr* outResult;
      SynchronizedCounter* counter;

      FhgfsOpsErr communicate();


   public:
      // getters & setters
      void setIsMirror(bool isMirror)
      {
         this->isMirror = isMirror;
      }

};

#endif /* CLOSECHUNKFILEWORK_H_ */
