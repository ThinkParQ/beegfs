#ifndef REQUESTEXCEEDEDQUOTAMSG_H_
#define REQUESTEXCEEDEDQUOTAMSG_H_


#include <common/net/message/NetMessage.h>
#include <common/storage/quota/QuotaData.h>
#include <common/Common.h>

class RequestExceededQuotaMsg: public NetMessageSerdes<RequestExceededQuotaMsg>
{
   public:
      RequestExceededQuotaMsg(QuotaDataType idType, QuotaLimitType exType)
      : BaseType(NETMSGTYPE_RequestExceededQuota)
      {
         this->quotaDataType = idType;
         this->exceededType = exType;
      }

      /**
       * For deserialization only
       */
      RequestExceededQuotaMsg() : BaseType(NETMSGTYPE_RequestExceededQuota)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->quotaDataType
            % obj->exceededType;
      }

   private:
      int32_t quotaDataType;
      int32_t exceededType;


   public:
      // getters and setters
      QuotaDataType getQuotaDataType()
      {
         return (QuotaDataType) this->quotaDataType;
      }

      QuotaLimitType getExceededType()
      {
         return (QuotaLimitType) this->exceededType;
      }
};

#endif /* REQUESTEXCEEDEDQUOTAMSG_H_ */
