#ifndef SETDIRPATTERNMSGEX_H_
#define SETDIRPATTERNMSGEX_H_

#include <storage/DirInode.h>
#include <common/storage/StorageErrors.h>
#include <common/net/message/storage/attribs/SetDirPatternMsg.h>

// set stripe pattern, called by fhgfs-ctl

class SetDirPatternMsgEx : public SetDirPatternMsg
{
   public:
      SetDirPatternMsgEx() : SetDirPatternMsg()
      {
      }

      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
         char* respBuf, size_t bufLen, HighResolutionStats* stats);
   
   protected:
   
   private:
      FhgfsOpsErr setPatternRec(EntryInfo* entryInfo, StripePattern* pattern);
      FhgfsOpsErr setPatternRoot(StripePattern* pattern);

};


#endif /*SETDIRPATTERNMSGEX_H_*/
