#ifndef SETCHUNKFILEATTRIBSWORK_H_
#define SETCHUNKFILEATTRIBSWORK_H_

#include <common/components/worker/Work.h>
#include <common/net/sock/Socket.h>
#include <common/storage/StorageDefinitions.h>
#include <common/storage/StorageErrors.h>
#include <common/storage/striping/ChunkFileInfo.h>
#include <common/storage/striping/StripePattern.h>
#include <common/toolkit/SynchronizedCounter.h>
#include <common/Common.h>


class SetChunkFileAttribsWork : public Work
{
   public:

      /**
       * @param pathInfo just a reference, so do not free it as long as you use this object!
       */
      SetChunkFileAttribsWork(const std::string& entryID, int validAttribs,
         SettableFileAttribs* attribs, bool enableCreation, StripePattern* pattern,
         uint16_t targetID, PathInfo* pathInfo, DynamicFileAttribs* outDynamicAttribs,
         FhgfsOpsErr* outResult, SynchronizedCounter* counter) :
         entryID(entryID), validAttribs(validAttribs), attribs(attribs),
         enableCreation(enableCreation), pattern(pattern), targetID(targetID),
         pathInfo(pathInfo), outDynamicAttribs(outDynamicAttribs),
         outResult(outResult), counter(counter), quotaChown(false),
         useBuddyMirrorSecond(false), msgUserID(NETMSG_DEFAULT_USERID)
      {
         // all assignments done in initializer list
      }

      virtual ~SetChunkFileAttribsWork() {}


      virtual void process(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen);


   private:
      std::string entryID;
      int validAttribs;
      SettableFileAttribs* attribs;
      bool enableCreation;
      StripePattern* pattern;
      uint16_t targetID;
      PathInfo* pathInfo;
      DynamicFileAttribs* outDynamicAttribs; // will hold the chunks dynamic attribs as stat'ed on
                                             // the storage server
      FhgfsOpsErr* outResult;
      SynchronizedCounter* counter;

      bool quotaChown;

      bool useBuddyMirrorSecond;

      unsigned msgUserID; // only used for msg header info

      FhgfsOpsErr communicate();


   public:
      // getters & setters
      void setQuotaChown(bool quotaChown)
      {
         this->quotaChown = quotaChown;
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

#endif /* SETCHUNKFILEATTRIBSWORK_H_ */
