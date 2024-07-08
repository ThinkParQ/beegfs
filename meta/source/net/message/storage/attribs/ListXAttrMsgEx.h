#ifndef LISTXATTRMSGEX_H_
#define LISTXATTRMSGEX_H_

#include <common/net/message/storage/attribs/ListXAttrMsg.h>
#include <common/net/message/storage/attribs/ListXAttrRespMsg.h>
#include <net/message/MirroredMessage.h>

class ListXAttrMsgResponseState : public MirroredMessageResponseState
{
   public:
      ListXAttrMsgResponseState() : size(0), returnCode(FhgfsOpsErr_INTERNAL)
      {
      }

      ListXAttrMsgResponseState(ListXAttrMsgResponseState&& other) :
         value(std::move(other.value)),
         size(other.size),
         returnCode(other.returnCode)
      {
      }

      void sendResponse(NetMessage::ResponseContext& ctx) override
      {
         ListXAttrRespMsg resp(value, size, returnCode);
         ctx.sendResponse(resp);
      }

      // ListXAttrMsgEx is transformed into a mirrored message to leverage MirroredMessage::lock(),
      // preventing races with operations such as unlink. However, forwarding this message to the
      // secondary is unnecessary. Overriding the changesObservableState() function to always
      // return false ensures that this message is never forwarded unnecessarily.
      bool changesObservableState() const override
      {
         return false;
      }

      void setListXAttrValue(const StringVector& value) { this->value = value; }
      void setSize(size_t size) { this->size = size; }
      void setListXAttrResult(FhgfsOpsErr result) { this->returnCode = result; }

    protected:
      uint32_t serializerTag() const override { return NETMSGTYPE_ListXAttr; }
      void serializeContents(Serializer& ser) const override {}

   private:
      StringVector value;
      size_t size;
      FhgfsOpsErr returnCode;
};

class ListXAttrMsgEx : public MirroredMessage<ListXAttrMsg, FileIDLock>
{
   public:
      typedef ListXAttrMsgResponseState ResponseState;
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

      const char* mirrorLogContext() const override { return "ListXAttrMsgEx/forward"; }
};

#endif /*LISTXATTRMSGEX_H_*/
