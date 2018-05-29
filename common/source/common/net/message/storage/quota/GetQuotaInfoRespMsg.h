#ifndef GETQUOTAINFORESPMSG_H_
#define GETQUOTAINFORESPMSG_H_


#include <common/net/message/NetMessage.h>
#include <common/storage/quota/QuotaData.h>
#include <common/storage/quota/Quota.h>
#include <common/Common.h>

#define GETQUOTAINFORESPMSG_MAX_SIZE_FOR_QUOTA_DATA   ( (unsigned)(NETMSG_MAX_PAYLOAD_SIZE - 1024) )
#define GETQUOTAINFORESPMSG_MAX_ID_COUNT              ( (unsigned) \
   ( (GETQUOTAINFORESPMSG_MAX_SIZE_FOR_QUOTA_DATA - sizeof(unsigned) ) / sizeof(QuotaData) ) )


class GetQuotaInfoRespMsg: public NetMessageSerdes<GetQuotaInfoRespMsg>
{
   public:
      /**
       * Constructor for quota limits (management server).
       */
      GetQuotaInfoRespMsg(QuotaDataList* quotaData)
         : BaseType(NETMSGTYPE_GetQuotaInfoResp), quotaData(quotaData),
           quotaInodeSupport(QuotaInodeSupport_UNKNOWN) {};

      /**
       * Constructor for quota data (storage server).
       */
      GetQuotaInfoRespMsg(QuotaDataList* quotaData, QuotaInodeSupport inodeSupport)
         : BaseType(NETMSGTYPE_GetQuotaInfoResp), quotaData(quotaData),
           quotaInodeSupport(inodeSupport) {};

      /**
       * For deserialization only
       */
      GetQuotaInfoRespMsg() : BaseType(NETMSGTYPE_GetQuotaInfoResp)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->quotaInodeSupport
            % serdes::backedPtr(obj->quotaData, obj->parsed.quotaData);
      }

   private:
      QuotaDataList* quotaData;
      uint32_t quotaInodeSupport;

      // for deserialization
      struct {
         QuotaDataList quotaData;
      } parsed;


   public:
      QuotaDataList& getQuotaDataList()
      {
         return *quotaData;
      }

      QuotaInodeSupport getQuotaInodeSupport()
      {
         return (QuotaInodeSupport)quotaInodeSupport;
      }
};

#endif /* GETQUOTAINFORESPMSG_H_ */
