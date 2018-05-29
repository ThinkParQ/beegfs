#ifndef META_MIRRORMESSAGERESPONSESTATE_H
#define META_MIRRORMESSAGERESPONSESTATE_H

#include <common/net/message/control/GenericResponseMsg.h>
#include <common/storage/EntryInfo.h>

class MirroredMessageResponseState
{
   public:
      virtual ~MirroredMessageResponseState()
      {
      }

      virtual void sendResponse(NetMessage::ResponseContext& ctx) = 0;

      virtual bool changesObservableState() const = 0;

      void serialize(Serializer& ser) const;

      static std::unique_ptr<MirroredMessageResponseState> deserialize(Deserializer& des);

   protected:
      virtual uint32_t serializerTag() const = 0;
      virtual void serializeContents(Serializer& ser) const = 0;
};

template<typename RespMsgT, uint32_t SerializerTag>
class ErrorCodeResponseState : public MirroredMessageResponseState
{
   public:
      explicit ErrorCodeResponseState(Deserializer& des)
      {
         des % serdes::as<int32_t>(result);
      }

      explicit ErrorCodeResponseState(FhgfsOpsErr result) : result(result) {}

      void sendResponse(NetMessage::ResponseContext& ctx) override
      {
         if (result == FhgfsOpsErr_COMMUNICATION)
            ctx.sendResponse(
                  GenericResponseMsg(
                     GenericRespMsgCode_INDIRECTCOMMERR,
                     "Communication with storage targets failed"));
         else
            ctx.sendResponse(RespMsgT(result));
      }

      bool changesObservableState() const override
      {
         return result == FhgfsOpsErr_SUCCESS;
      }

      FhgfsOpsErr getResult() const { return result; }

   protected:
      uint32_t serializerTag() const override { return SerializerTag; }

      void serializeContents(Serializer& ser) const override
      {
         ser % serdes::as<int32_t>(result);
      }

   private:
      FhgfsOpsErr result;
};

template<typename RespMsgT, uint32_t SerializerTag>
class ErrorAndEntryResponseState : public MirroredMessageResponseState
{
   public:
      explicit ErrorAndEntryResponseState(Deserializer& des)
      {
         des
            % serdes::as<int32_t>(result)
            % info;
      }

      ErrorAndEntryResponseState(FhgfsOpsErr result, EntryInfo info)
         : result(result), info(std::move(info))
      {
      }

      void sendResponse(NetMessage::ResponseContext& ctx) override
      {
         if (result == FhgfsOpsErr_COMMUNICATION)
            ctx.sendResponse(
                  GenericResponseMsg(
                     GenericRespMsgCode_INDIRECTCOMMERR,
                     "Communication with storage targets failed"));
         else
            ctx.sendResponse(RespMsgT(result, &info));
      }

      bool changesObservableState() const override
      {
         return result == FhgfsOpsErr_SUCCESS;
      }

      FhgfsOpsErr getResult() const { return result; }

   protected:
      uint32_t serializerTag() const override { return SerializerTag; }

      void serializeContents(Serializer& ser) const override
      {
         ser
            % serdes::as<int32_t>(result)
            % info;
      }

   private:
      FhgfsOpsErr result;
      EntryInfo info;
};

#endif
