#ifndef STATMSGEX_H_
#define STATMSGEX_H_

#include <storage/DirInode.h>
#include <common/storage/StorageErrors.h>
#include <common/net/message/storage/attribs/StatMsg.h>

// The "getattr" operation for linux-kernel files ystems

class StatMsgEx : public StatMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);

   private:
      FhgfsOpsErr statRoot(StatData& outStatData);
};

#endif /*STATMSGEX_H_*/
