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

         des % overWrittenEntryInfo;
      }

      MovingFileInsertResponseState(FhgfsOpsErr result, unsigned inodeBufLen,
            std::unique_ptr<char[]> inodeBuf, EntryInfo entryInfo)
         : result(result), inodeBufLen(inodeBufLen), inodeBuf(std::move(inodeBuf)),
            overWrittenEntryInfo(entryInfo)
      {
      }

      void sendResponse(NetMessage::ResponseContext& ctx) override
      {
         ctx.sendResponse(MovingFileInsertRespMsg(result, inodeBufLen, inodeBuf.get(), overWrittenEntryInfo));
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
      EntryInfo overWrittenEntryInfo;
};

class MovingFileInsertMsgEx : public MirroredMessage<MovingFileInsertMsg,
   std::tuple<FileIDLock, FileIDLock, FileIDLock, ParentNameLock>>
{
   public:
      typedef MovingFileInsertResponseState ResponseState;

      virtual bool processIncoming(ResponseContext& ctx) override;

      std::tuple<FileIDLock, FileIDLock, FileIDLock, ParentNameLock>
         lock(EntryLockStore& store) override;

      bool isMirrored() override { return getToDirInfo()->getIsBuddyMirrored(); }

   private:
      ResponseContext* rctx;

      StringVector xattrNames;
      EntryInfo newFileInfo;
      MsgHelperXAttr::StreamXAttrState streamState;

      std::unique_ptr<MirroredMessageResponseState> executeLocally(ResponseContext& ctx,
         bool isSecondary) override;

      void forwardToSecondary(ResponseContext& ctx) override;

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
