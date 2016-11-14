#ifndef TRUNCLOCALFILEWORK_H_
#define TRUNCLOCALFILEWORK_H_

#include <common/components/worker/Work.h>
#include <common/net/sock/Socket.h>
#include <common/storage/StorageErrors.h>
#include <common/toolkit/SynchronizedCounter.h>
#include <common/storage/striping/StripeNodeFileInfo.h>
#include <common/Common.h>


/**
 * Truncate file on storage servers
 */
class TruncLocalFileWork : public Work
{
   public:
      /**
       * @param outDynAttribs may be NULL if caller is not interested
       */
      TruncLocalFileWork(std::string entryID, int64_t filesize, uint16_t targetID,
         DynamicFileAttribs *outDynAttribs, FhgfsOpsErr* outResult, SynchronizedCounter* counter) :
         entryID(entryID), filesize(filesize), targetID(targetID), mirroredFromTargetID(0),
         outDynAttribs(outDynAttribs), outResult(outResult), counter(counter)
      {
         // all assignments done in initializer list
      }

      virtual ~TruncLocalFileWork() {}


      virtual void process(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen);


   private:
      std::string entryID;
      int64_t filesize; // already converted to storage node's local file size
      uint16_t targetID;
      uint16_t mirroredFromTargetID; // chunk file is a mirrored version from this targetID
      DynamicFileAttribs* outDynAttribs;
      FhgfsOpsErr* outResult;
      SynchronizedCounter* counter;

      FhgfsOpsErr communicate();


   public:
      // getters & setters
      void setMirroredFromTargetID(uint16_t mirroredFromTargetID)
      {
         this->mirroredFromTargetID = mirroredFromTargetID;
      }
};

#endif /* TRUNCLOCALFILEWORK_H_ */
