#ifndef GETXATTRMSGEX_H_
#define GETXATTRMSGEX_H_

#include <common/net/message/storage/attribs/GetXAttrMsg.h>
#include <common/net/message/storage/attribs/GetXAttrRespMsg.h>
#include <net/message/MirroredMessage.h>

class GetXAttrMsgResponseState : public MirroredMessageResponseState
{
   public:
      GetXAttrMsgResponseState() : size(0), returnCode(FhgfsOpsErr_INTERNAL)
      {
      }

      GetXAttrMsgResponseState(GetXAttrMsgResponseState&& other) :
         value(std::move(other.value)),
         size(other.size),
         returnCode(other.returnCode)
      {
      }

      void sendResponse(NetMessage::ResponseContext& ctx) override
      {
         GetXAttrRespMsg resp(value, size, returnCode);
         ctx.sendResponse(resp);
      }

      // GetXAttrMsgEx is converted into a mirrored message to utilize MirroredMessage::lock(),
      // thereby preventing races with operations like unlink. However, forwarding this message
      // to the secondary is unnecessary. Overriding the changesObservableState() function to
      // always return false ensures that this message is never forwarded unnecessarily.
      bool changesObservableState() const override
      {
         return false;
      }

      void setGetXAttrValue(const CharVector& value) { this->value = value; }
      void setSize(size_t size) { this->size = size; }
      void setGetXAttrResult(FhgfsOpsErr result) { this->returnCode = result; }

   protected:
      uint32_t serializerTag() const override { return NETMSGTYPE_GetXAttr; }
      void serializeContents(Serializer& ser) const override {}

   private:
      CharVector value;
      size_t size;
      FhgfsOpsErr returnCode;
};

class GetXAttrMsgEx : public MirroredMessage<GetXAttrMsg, FileIDLock>
{
   public:
      typedef GetXAttrMsgResponseState ResponseState;
      virtual bool processIncoming(ResponseContext& ctx);
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

      const char* mirrorLogContext() const override { return "GetXAttrMsgEx/forward"; }
};

#endif /*GETXATTRMSGEX_H_*/
