#ifndef TRUNCCHUNKFILEWORK_H_
#define TRUNCCHUNKFILEWORK_H_

#include <common/components/worker/Work.h>
#include <common/net/sock/Socket.h>
#include <common/storage/striping/StripePattern.h>
#include <common/storage/StorageErrors.h>
#include <common/toolkit/SynchronizedCounter.h>
#include <common/storage/striping/ChunkFileInfo.h>
#include <common/Common.h>


/**
 * Truncate file on storage servers
 */
class TruncChunkFileWork : public Work
{
   public:
      /**
       * @param outDynAttribs may be NULL if caller is not interested
       */
      TruncChunkFileWork(const std::string& entryID, int64_t filesize, StripePattern* pattern,
         uint16_t targetID, PathInfo* pathInfo, DynamicFileAttribs *outDynAttribs,
         FhgfsOpsErr* outResult, SynchronizedCounter* counter) :
         entryID(entryID), filesize(filesize), pattern(pattern), targetID(targetID),
         pathInfo(pathInfo), outDynAttribs(outDynAttribs), outResult(outResult), counter(counter),
         useQuota(false), useBuddyMirrorSecond(false), msgUserID(NETMSG_DEFAULT_USERID)
      {
         // all assignments done in initializer list
      }

      virtual ~TruncChunkFileWork() {}


      virtual void process(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen);


   private:
      std::string entryID;
      int64_t filesize; // already converted to storage node's local file size
      StripePattern* pattern;
      uint16_t targetID;
      PathInfo* pathInfo; // note: not owned by this object
      DynamicFileAttribs* outDynAttribs;
      FhgfsOpsErr* outResult;
      SynchronizedCounter* counter;

      unsigned userID;
      unsigned groupID;
      bool useQuota;

      bool useBuddyMirrorSecond;

      unsigned msgUserID; // only used for msg header info

      FhgfsOpsErr communicate();



   public:
      // getters & setters
      void setUserdataForQuota(unsigned userID, unsigned groupID)
      {
         this->useQuota = true;
         this->userID = userID;
         this->groupID = groupID;
      }

      void setMsgUserID(unsigned msgUserID)
      {
         this->msgUserID = msgUserID;
      }

      void setUseBuddyMirrorSecond()
      {
         this->useBuddyMirrorSecond = true;
      }
};

#endif /* TRUNCCHUNKFILEWORK_H_ */
