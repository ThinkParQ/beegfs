#ifndef REQUESTEXCEEDEDQUOTARESPMSG_H_
#define REQUESTEXCEEDEDQUOTARESPMSG_H_


#include <common/net/message/storage/quota/SetExceededQuotaMsg.h>
#include <common/storage/quota/QuotaData.h>
#include <common/Common.h>


#define REQUESTEXCEEDEDQUOTARESPMSG_MAX_SIZE_FOR_QUOTA_DATA    (\
   SETEXCEEDEDQUOTAMSG_MAX_SIZE_FOR_QUOTA_DATA)
#define REQUESTEXCEEDEDQUOTARESPMSG_MAX_ID_COUNT               (SETEXCEEDEDQUOTAMSG_MAX_ID_COUNT)


class RequestExceededQuotaRespMsg: public SetExceededQuotaMsg
{
   public:
      RequestExceededQuotaRespMsg(QuotaDataType idType, QuotaLimitType exType, int error)
      : SetExceededQuotaMsg(idType, exType, NETMSGTYPE_RequestExceededQuotaResp), error(error)
      {
      }

      /**
       * For deserialization only
       */
      RequestExceededQuotaRespMsg() : SetExceededQuotaMsg(NETMSGTYPE_RequestExceededQuotaResp),
         error(FhgfsOpsErr_INVAL)
      {
      }

      // overloads to use the serialize() definition of this class
      void serializePayload(Serializer& ser) const
      {
         ser % *this;
      }

      // overloads to use the serialize() definition of this class
      bool deserializePayload(const char* buf, size_t bufLen)
      {
         Deserializer des(buf, bufLen);
         des % *this;
         return des.good();
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::base<SetExceededQuotaMsg>(obj)
            % obj->error;
      }

      FhgfsOpsErr getError()
      {
         return (FhgfsOpsErr)error;
      }

      void setError(FhgfsOpsErr newError)
      {
         error = newError;
      }

   private:
      int error;

};

#endif /* REQUESTEXCEEDEDQUOTARESPMSG_H_ */
