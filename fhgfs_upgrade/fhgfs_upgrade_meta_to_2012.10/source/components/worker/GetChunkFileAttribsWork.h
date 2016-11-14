#ifndef GETCHUNKFILEATTRIBSWORK_H_
#define GETCHUNKFILEATTRIBSWORK_H_

#include <common/components/worker/Work.h>
#include <common/net/sock/Socket.h>
#include <common/storage/StorageErrors.h>
#include <common/toolkit/SynchronizedCounter.h>
#include <common/storage/striping/StripeNodeFileInfo.h>
#include <common/Common.h>


class GetChunkFileAttribsWork : public Work
{
   public:
      GetChunkFileAttribsWork(std::string entryID, uint16_t targetID,
         DynamicFileAttribs *outDynAttribs, FhgfsOpsErr* outResult, SynchronizedCounter* counter) :
         entryID(entryID), targetID(targetID),
         outDynAttribs(outDynAttribs), outResult(outResult), counter(counter)
      {
      }

      virtual ~GetChunkFileAttribsWork()
      {
      }


      virtual void process(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen);


   private:
      std::string entryID;
      uint16_t targetID;
      DynamicFileAttribs* outDynAttribs;
      FhgfsOpsErr* outResult;
      SynchronizedCounter* counter;

      FhgfsOpsErr communicate();
};

#endif /* GETCHUNKFILEATTRIBSWORK_H_ */
