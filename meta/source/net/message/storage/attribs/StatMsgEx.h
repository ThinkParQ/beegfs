#ifndef STATMSGEX_H_
#define STATMSGEX_H_

#include <storage/DirInode.h>
#include <common/storage/StorageErrors.h>
#include <common/net/message/storage/attribs/StatMsg.h>
#include <net/message/MirroredMessage.h>

// The "getattr" operation for linux-kernel filesystems

class StatMsgResponseState : public MirroredMessageResponseState
{
   public:
      StatMsgResponseState() : result(FhgfsOpsErr_INTERNAL), hasParentInfo(false) {}

      explicit StatMsgResponseState(Deserializer& des)
      {
         serialize(this, des);
      }

      StatMsgResponseState(StatMsgResponseState&& other) :
         result(other.result),
         statData(other.statData),
         parentNodeID(other.parentNodeID),
         parentEntryID(other.parentEntryID),
         hasParentInfo(other.hasParentInfo)
      {}

      void sendResponse(NetMessage::ResponseContext& ctx) override
      {
         StatRespMsg resp(result, statData);

         if (this->hasParentInfo)
         {
            resp.addParentInfo(parentNodeID, parentEntryID);
         }

         ctx.sendResponse(resp);
      }

      // StatMsgEx is converted to mirrored message to leverage locking part of
      // mirrored messages to prevent races with unlink, open etc.
      //
      // Always return false from changeObservableState() to prohibit forwarding
      // to secondary. See MirroredMessage::finishOperation() for more details.
      bool changesObservableState() const override
      {
         return false;
      }

      void setParentInfo(NumNodeID nodeID, const std::string& parentEntryID)
      {
         this->hasParentInfo = true;
         this->parentNodeID = nodeID;
         this->parentEntryID = parentEntryID;
      }

      void setStatData(const StatData& statData)
      {
         this->statData = statData;
      }

      void setStatResult(FhgfsOpsErr statRes) { this->result = statRes; }

   protected:
      uint32_t serializerTag() const override { return NETMSGTYPE_Stat; }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->result
            % obj->statData.serializeAs(StatDataFormat_NET);

         if (obj->hasParentInfo)
         {
            ctx
               % serdes::stringAlign4(obj->parentEntryID)
               % obj->parentNodeID;
         }
      }

      void serializeContents(Serializer& ser) const override
      {
         serialize(this, ser);
      }

   private:
      FhgfsOpsErr result;
      StatData statData;
      NumNodeID parentNodeID;
      std::string parentEntryID;
      bool hasParentInfo;
};

class StatMsgEx: public MirroredMessage<StatMsg, FileIDLock>
{
   public:
      typedef StatMsgResponseState ResponseState;

      virtual bool processIncoming(ResponseContext& ctx) override;

      FileIDLock lock(EntryLockStore& store) override;

      bool isMirrored() override
      {
         return getEntryInfo()->getIsBuddyMirrored();
      }

   private:
      std::unique_ptr<MirroredMessageResponseState> executeLocally(ResponseContext& ctx,
         bool isSecondary) override;

      void forwardToSecondary(ResponseContext& ctx) override {}

      FhgfsOpsErr processSecondaryResponse(NetMessage& resp) override
      {
         return FhgfsOpsErr_SUCCESS;
      }

      const char* mirrorLogContext() const override { return "StatMsgEx/forward"; }

      FhgfsOpsErr statRoot(StatData& outStatData);
};

#endif /*STATMSGEX_H_*/
