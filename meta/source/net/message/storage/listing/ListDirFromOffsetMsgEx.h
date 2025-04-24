#pragma once

#include <storage/MetaStore.h>
#include <common/storage/EntryInfo.h>
#include <common/storage/StorageErrors.h>
#include <common/net/message/storage/listing/ListDirFromOffsetMsg.h>


class ListDirFromOffsetMsgEx : public ListDirFromOffsetMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);

   private:
      FhgfsOpsErr listDirIncremental(EntryInfo* entryInfo, StringList* outNames,
         UInt8List* outEntryTypes, StringList* outEntryIDs, Int64List* outServerOffsets,
         int64_t* outNewOffset);
};


