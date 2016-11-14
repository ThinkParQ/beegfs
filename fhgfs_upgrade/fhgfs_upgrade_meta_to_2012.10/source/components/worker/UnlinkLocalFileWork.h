#ifndef UNLINKLOCALFILEWORK_H_
#define UNLINKLOCALFILEWORK_H_

#include <common/components/worker/Work.h>
#include <common/net/sock/Socket.h>
#include <common/storage/StorageErrors.h>
#include <common/toolkit/SynchronizedCounter.h>
#include <common/storage/striping/StripeNodeFileInfo.h>
#include <common/Common.h>


class UnlinkLocalFileWork : public Work
{
   public:
      UnlinkLocalFileWork(std::string entryID, uint16_t targetID,
         FhgfsOpsErr* outResult, SynchronizedCounter* counter) :
         entryID(entryID), targetID(targetID), mirroredFromTargetID(0),
         outResult(outResult), counter(counter)
      {
         // all assignments done in initializer list
      }

      virtual ~UnlinkLocalFileWork() {}


      virtual void process(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen);


   private:
      std::string entryID;
      uint16_t targetID;
      uint16_t mirroredFromTargetID; // chunk file is a mirrored version from this targetID
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

#endif /* UNLINKLOCALFILEWORK_H_ */
