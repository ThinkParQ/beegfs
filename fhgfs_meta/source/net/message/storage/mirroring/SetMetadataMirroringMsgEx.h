#ifndef SETMETADATAMIRRORINGMSGEX_H_
#define SETMETADATAMIRRORINGMSGEX_H_

#include <common/storage/StorageErrors.h>
#include <common/net/message/storage/mirroring/SetMetadataMirroringMsg.h>


class SetMetadataMirroringMsgEx : public SetMetadataMirroringMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);

      static FhgfsOpsErr setMirroring();

   private:
      static FhgfsOpsErr moveRootInode(bool revertMove);
      static FhgfsOpsErr moveRootDirectory(bool revertMove);
      static FhgfsOpsErr informMgmt(uint16_t newRootNodeID);
};

#endif /*SETMETADATAMIRRORINGMSGEX_H_*/
