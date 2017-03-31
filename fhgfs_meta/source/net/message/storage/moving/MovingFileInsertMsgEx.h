#ifndef MOVINGFILEINSERTMSGEX_H_
#define MOVINGFILEINSERTMSGEX_H_

#include <common/storage/StorageErrors.h>
#include <common/net/message/storage/moving/MovingFileInsertMsg.h>
#include <common/net/message/storage/moving/MovingFileInsertRespMsg.h>
#include <net/message/MirroredMessage.h>
#include <storage/MetaStore.h>
#include <net/msghelpers/MsgHelperXAttr.h>

// this class is used on the server where the file is moved to

class MovingFileInsertResponseState : public MirroredMessageResponseState
{
   public:
      explicit MovingFileInsertResponseState(FhgfsOpsErr result)
         : result(result), inodeBufLen(0)
      {
      }

      explicit MovingFileInsertResponseState(Deserializer& des)
      {
         des
            % serdes::as<int32_t>(result)
            % inodeBufLen;

         if (inodeBufLen > META_SERBUF_SIZE)
            des.setBad();
         else
         {
            inodeBuf.reset(new char[inodeBufLen]);
            des.getBlock(inodeBuf.get(), inodeBufLen);
         }
      }

      MovingFileInsertResponseState(FhgfsOpsErr result, unsigned inodeBufLen,
            std::unique_ptr<char[]> inodeBuf)
         : result(result), inodeBufLen(inodeBufLen), inodeBuf(std::move(inodeBuf))
      {
      }

      void sendResponse(NetMessage::ResponseContext& ctx) override
      {
         ctx.sendResponse(MovingFileInsertRespMsg(result, inodeBufLen, inodeBuf.get()));
      }

      bool changesObservableState() const override
      {
         return result == FhgfsOpsErr_SUCCESS;
      }

   protected:
      uint32_t serializerTag() const override { return NETMSGTYPE_MovingFileInsert; }

      void serializeContents(Serializer& ser) const override
      {
         ser
            % serdes::as<int32_t>(result)
            % inodeBufLen;

         ser.putBlock(inodeBuf.get(), inodeBufLen);
      }

   private:
      FhgfsOpsErr result;
      unsigned inodeBufLen;
      std::unique_ptr<char[]> inodeBuf;
};

class MovingFileInsertMsgEx : public MirroredMessage<MovingFileInsertMsg,
   std::tuple<FileIDLock, FileIDLock, DirIDLock>>
{
   public:
      typedef MovingFileInsertResponseState ResponseState;

      virtual bool processIncoming(ResponseContext& ctx) override;

      std::tuple<FileIDLock, FileIDLock, DirIDLock> lock(EntryLockStore& store) override
      {
         // the created file need not be locked, because only the requestor of the move operations
         // knows the new parent of the file, and only knows it for certain once the origin server
         // has acknowledged the operation.
         // we must not lock the directory if it is owned by the current node. if it is, the
         // current message was also sent by the local node, specifically by a RmDirMsgEx, which
         // also locks the directory for write
         uint16_t localID = Program::getApp()->getMetaBuddyGroupMapper()->getLocalGroupID();
         if (getToDirInfo()->getOwnerNodeID().val() == localID)
            return {};

         return std::make_tuple(
               FileIDLock(), // new file inode
               FileIDLock(), // (maybe) overwritten file inode
               DirIDLock(&store, getToDirInfo()->getEntryID(), true));
      }

      bool isMirrored() override { return getToDirInfo()->getIsBuddyMirrored(); }

   private:
      StringVector xattrNames;
      EntryInfo newFileInfo;
      MsgHelperXAttr::StreamXAttrState streamState;

      std::unique_ptr<MirroredMessageResponseState> executeLocally(ResponseContext& ctx,
         bool isSecondary) override;

      bool forwardToSecondary(ResponseContext& ctx) override;

      void prepareMirrorRequestArgs(RequestResponseArgs& args) override
      {
         if (isMsgHeaderFeatureFlagSet(MOVINGFILEINSERTMSG_FLAG_HAS_XATTRS))
         {
            streamState = {newFileInfo, xattrNames};
            registerStreamoutHook(args, streamState);
         }
      }

      FhgfsOpsErr processSecondaryResponse(NetMessage& resp) override
      {
         return static_cast<MovingFileInsertRespMsg&>(resp).getResult();
      }

      const char* mirrorLogContext() const override { return "MovingFileInsertMsgEx/forward"; }
};

#endif /*MOVINGFILEINSERTMSGEX_H_*/
