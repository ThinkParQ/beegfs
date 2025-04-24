#pragma once

#include <common/storage/StorageErrors.h>
#include <common/net/message/storage/creating/RmDirEntryMsg.h>
#include <storage/MetaStore.h>

// A repair operation, called from fhgfs-ctl

class RmDirEntryMsgEx : public RmDirEntryMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);

   private:
      FhgfsOpsErr rmDirEntry(EntryInfo* parentInfo, std::string& entryName);
};

