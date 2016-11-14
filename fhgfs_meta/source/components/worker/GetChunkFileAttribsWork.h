#ifndef GETCHUNKFILEATTRIBSWORK_H_
#define GETCHUNKFILEATTRIBSWORK_H_

#include <common/components/worker/Work.h>
#include <common/net/sock/Socket.h>
#include <common/storage/striping/StripePattern.h>
#include <common/storage/PathInfo.h>
#include <common/storage/StorageErrors.h>
#include <common/toolkit/SynchronizedCounter.h>
#include <common/storage/striping/ChunkFileInfo.h>
#include <common/Common.h>


class GetChunkFileAttribsWork : public Work
{
   public:

      /**
       * @param pathInfo: Only as reference pointer, not owned by this object
       */
      GetChunkFileAttribsWork(const std::string& entryID, StripePattern* pattern, uint16_t targetID,
         PathInfo* pathInfo, DynamicFileAttribs *outDynAttribs, FhgfsOpsErr* outResult,
         SynchronizedCounter* counter) : entryID(entryID), pattern(pattern), targetID(targetID),
         pathInfo(pathInfo), outDynAttribs(outDynAttribs), outResult(outResult), counter(counter),
         useBuddyMirrorSecond(false), msgUserID(NETMSG_DEFAULT_USERID)
      {
         // all assignments done in initializer list
      }

      virtual ~GetChunkFileAttribsWork()
      {
      }


      virtual void process(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen);


   private:
      std::string entryID;
      StripePattern* pattern;
      uint16_t targetID;
      PathInfo *pathInfo; // only as reference ptr, not owned by this object!
      DynamicFileAttribs* outDynAttribs;
      FhgfsOpsErr* outResult;
      SynchronizedCounter* counter;

      bool useBuddyMirrorSecond;

      unsigned msgUserID; // only used for msg header info

      FhgfsOpsErr communicate();

   public:
      void setMsgUserID(unsigned msgUserID)
      {
         this->msgUserID = msgUserID;
      }

      void setUseBuddyMirrorSecond()
      {
         this->useBuddyMirrorSecond = true;
      }

};

#endif /* GETCHUNKFILEATTRIBSWORK_H_ */
