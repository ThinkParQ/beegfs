#ifndef SETLOCALFILEATTRIBSWORK_H_
#define SETLOCALFILEATTRIBSWORK_H_

#include <common/components/worker/Work.h>
#include <common/net/sock/Socket.h>
#include <common/storage/StorageDefinitions.h>
#include <common/storage/StorageErrors.h>
#include <common/toolkit/SynchronizedCounter.h>
#include <common/storage/striping/StripeNodeFileInfo.h>
#include <common/Common.h>


class SetLocalFileAttribsWork : public Work
{
   public:
      SetLocalFileAttribsWork(std::string entryID, int validAttribs, SettableFileAttribs* attribs,
         bool enableCreation, uint16_t targetID, FhgfsOpsErr* outResult,
         SynchronizedCounter* counter) :
         entryID(entryID), validAttribs(validAttribs), attribs(attribs),
         enableCreation(enableCreation), targetID(targetID), mirroredFromTargetID(0),
         outResult(outResult), counter(counter)
      {
         // all assignments done in initializer list
      }

      virtual ~SetLocalFileAttribsWork() {}


      virtual void process(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen);


   private:
      std::string entryID;
      int validAttribs;
      SettableFileAttribs* attribs;
      bool enableCreation;
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

#endif /* SETLOCALFILEATTRIBSWORK_H_ */
