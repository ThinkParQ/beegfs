#ifndef MOVINGDIRINSERTMSGEX_H_
#define MOVINGDIRINSERTMSGEX_H_

#include <common/storage/StorageErrors.h>
#include <common/net/message/storage/moving/MovingDirInsertMsg.h>
#include <common/net/message/storage/moving/MovingDirInsertRespMsg.h>
#include <storage/MetaStore.h>
#include <net/message/MirroredMessage.h>

// Move directory to another meta-data server

class MovingDirInsertMsgEx : public MirroredMessage<MovingDirInsertMsg, DirIDLock>
{
   public:
      typedef ErrorCodeResponseState<MovingDirInsertRespMsg, NETMSGTYPE_MovingDirInsert>
         ResponseState;

      virtual bool processIncoming(ResponseContext& ctx) override;

      DirIDLock lock(EntryLockStore& store) override
      {
         // we must not lock the directory if it is owned by the current node. if it is, the
         // current message was also sent by the local node, specifically by a RmDirMsgEx, which
         // also locks the directory for write
         uint16_t localID = Program::getApp()->getMetaBuddyGroupMapper()->getLocalGroupID();
         if (getToDirInfo()->getOwnerNodeID().val() == localID)
            return {};

         return {&store, getToDirInfo()->getEntryID(), true};
      }

      bool isMirrored() override { return getToDirInfo()->getIsBuddyMirrored(); }

   private:
      std::unique_ptr<MirroredMessageResponseState> executeLocally(ResponseContext& ctx,
         bool isSecondary) override;

      bool forwardToSecondary(ResponseContext& ctx) override;

      FhgfsOpsErr processSecondaryResponse(NetMessage& resp) override
      {
         return static_cast<MovingDirInsertRespMsg&>(resp).getResult();
      }

      const char* mirrorLogContext() const override { return "MovingDirInsertMsgEx/forward"; }
};

#endif /*MOVINGDIRINSERTMSGEX_H_*/
