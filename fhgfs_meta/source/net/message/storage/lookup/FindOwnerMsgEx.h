#ifndef FINDOWNERMSGEX_H_
#define FINDOWNERMSGEX_H_

#include <common/net/message/storage/lookup/FindOwnerMsg.h>
#include <common/storage/StorageDefinitions.h>
#include <common/storage/StorageErrors.h>
#include <common/toolkit/MetadataTk.h>
#include <common/Common.h>
#include <storage/MetaStore.h>

// Get the meta-data server of a file

class FindOwnerMsgEx : public FindOwnerMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);

   private:
      FhgfsOpsErr findOwner(EntryInfoWithDepth* outInfo);
   
};


#endif /*FINDOWNERMSGEX_H_*/
