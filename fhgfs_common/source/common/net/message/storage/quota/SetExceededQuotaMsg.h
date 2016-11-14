#ifndef SETEXCEEDEDQUOTAMSG_H_
#define SETEXCEEDEDQUOTAMSG_H_


#include <common/net/message/NetMessage.h>
#include <common/storage/quota/QuotaData.h>
#include <common/Common.h>


#define SETEXCEEDEDQUOTAMSG_MAX_SIZE_FOR_QUOTA_DATA   ( (unsigned)(NETMSG_MAX_PAYLOAD_SIZE - 1024) )
#define SETEXCEEDEDQUOTAMSG_MAX_ID_COUNT              ( (unsigned) \
   ( (SETEXCEEDEDQUOTAMSG_MAX_SIZE_FOR_QUOTA_DATA - (2 * sizeof(int) ) ) / sizeof(unsigned) ) )


class SetExceededQuotaMsg: public NetMessageSerdes<SetExceededQuotaMsg>
{
   friend class TestSerialization;

   public:
      SetExceededQuotaMsg(QuotaDataType idType, QuotaLimitType exType)
      : BaseType(NETMSGTYPE_SetExceededQuota)
      {
         this->quotaDataType = idType;
         this->exceededType = exType;
      }

      SetExceededQuotaMsg(QuotaDataType idType, QuotaLimitType exType, unsigned short msgType)
      : BaseType(msgType)
      {
         this->quotaDataType = idType;
         this->exceededType = exType;
      }

      /**
       * For deserialization only
       */
      SetExceededQuotaMsg() : BaseType(NETMSGTYPE_SetExceededQuota)
      {
      }

      /**
       * For deserialization only
       */
      SetExceededQuotaMsg(unsigned short msgType) : BaseType(msgType)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->quotaDataType
            % obj->exceededType
            % obj->exceededQuotaIDs;
      }

   private:
      int32_t quotaDataType;         // QuotaDataType
      int32_t exceededType;          // ExceededType
      UIntList exceededQuotaIDs;

   public:
      //getters and setters

      QuotaDataType getQuotaDataType()
      {
         return (QuotaDataType) this->quotaDataType;
      }

      QuotaLimitType getExceededType()
      {
         return (QuotaLimitType) this->exceededType;
      }

      UIntList* getExceededQuotaIDs()
      {
         return &this->exceededQuotaIDs;
      }
};

#endif /* SETEXCEEDEDQUOTAMSG_H_ */
