#ifndef SETATTRMSGEX_H_
#define SETATTRMSGEX_H_

#include <storage/MetaStore.h>
#include <common/storage/StorageErrors.h>
#include <common/net/message/storage/attribs/SetAttrMsg.h>
#include <common/net/message/storage/attribs/SetAttrRespMsg.h>
#include <net/message/MirroredMessage.h>

class SetAttrMsgEx : public MirroredMessage<SetAttrMsg, std::tuple<FileIDLock, FileIDLock>>
{
   public:
      typedef ErrorCodeResponseState<SetAttrRespMsg, NETMSGTYPE_SetAttr> ResponseState;

      virtual bool processIncoming(ResponseContext& ctx) override;

      std::tuple<FileIDLock, FileIDLock> lock(EntryLockStore& store) override;

      bool isMirrored() override { return getEntryInfo()->getIsBuddyMirrored(); }

   private:
      std::unique_ptr<MirroredMessageResponseState> executeLocally(ResponseContext& ctx,
         bool isSecondary) override;
      void forwardToSecondary(ResponseContext& ctx) override;

      FhgfsOpsErr setAttrRoot();
      FhgfsOpsErr setChunkFileAttribs(FileInode& file, bool requestDynamicAttribs);
      FhgfsOpsErr setChunkFileAttribsSequential(FileInode& inode, bool requestDynamicAttribs);
      FhgfsOpsErr setChunkFileAttribsParallel(FileInode& inode, bool requestDynamicAttribs);

      FhgfsOpsErr processSecondaryResponse(NetMessage& resp) override
      {
         return (FhgfsOpsErr) static_cast<SetAttrRespMsg&>(resp).getValue();
      }

      const char* mirrorLogContext() const override { return "SetAttrMsgEx/forward"; }
};


#endif /*SETATTRMSGEX_H_*/
