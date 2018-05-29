#ifndef GETENTRYINFO_H_
#define GETENTRYINFO_H_

#include <storage/DirInode.h>
#include <common/storage/StorageErrors.h>
#include <common/net/message/storage/attribs/GetEntryInfoMsg.h>

class GetEntryInfoMsgEx : public GetEntryInfoMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);

   private:
      FhgfsOpsErr getInfo(EntryInfo* entryInfo, StripePattern** outPattern, PathInfo* outPathInfo);
      FhgfsOpsErr getRootInfo(StripePattern** outPattern);

};


#endif /*GETENTRYINFO_H_*/
