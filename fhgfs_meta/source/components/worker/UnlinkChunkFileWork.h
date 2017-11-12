#ifndef UNLINKCHUNKFILEWORK_H_
#define UNLINKCHUNKFILEWORK_H_

#include <common/components/worker/Work.h>
#include <common/net/sock/Socket.h>
#include <common/storage/striping/StripePattern.h>
#include <common/storage/PathInfo.h>
#include <common/storage/StorageErrors.h>
#include <common/toolkit/SynchronizedCounter.h>
#include <common/storage/striping/ChunkFileInfo.h>
#include <common/Common.h>


class UnlinkChunkFileWork : public Work
{
   public:

      /**
       * @param pathInfo just a reference, so do not free it as long as you use this object!
       */
      UnlinkChunkFileWork(const std::string& entryID, StripePattern* pattern, uint16_t targetID,
         PathInfo* pathInfo, FhgfsOpsErr* outResult, SynchronizedCounter* counter) :
         entryID(entryID), pattern(pattern), targetID(targetID), pathInfo(pathInfo),
         outResult(outResult), counter(counter), useBuddyMirrorSecond(false),
         msgUserID(NETMSG_DEFAULT_USERID)
      {
         // all assignments done in initializer list
      }

      virtual ~UnlinkChunkFileWork() {}


      virtual void process(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen);


   private:
      std::string entryID;
      StripePattern* pattern;
      uint16_t targetID;
      PathInfo* pathInfo;
      FhgfsOpsErr* outResult;
      SynchronizedCounter* counter;

      bool useBuddyMirrorSecond;

      unsigned msgUserID;

      FhgfsOpsErr communicate();


   public:
      // getters & setters
      void setMsgUserID(unsigned msgUserID)
      {
         this->msgUserID = msgUserID;
      }

      void setUseBuddyMirrorSecond()
      {
         this->useBuddyMirrorSecond = true;
      }
};

#endif /* UNLINKCHUNKFILEWORK_H_ */
