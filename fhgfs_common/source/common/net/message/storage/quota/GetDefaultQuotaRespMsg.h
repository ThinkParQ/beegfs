#ifndef COMMON_NET_MESSAGE_STORAGE_QUOTA_GETDEFAULTQUOTARESPMSG_H_
#define COMMON_NET_MESSAGE_STORAGE_QUOTA_GETDEFAULTQUOTARESPMSG_H_


#include <common/net/message/NetMessage.h>
#include <common/storage/quota/QuotaDefaultLimits.h>



class GetDefaultQuotaRespMsg: public NetMessageSerdes<GetDefaultQuotaRespMsg>
{
   public:
      /**
       * For deserialization only
       */
      GetDefaultQuotaRespMsg() : BaseType(NETMSGTYPE_GetDefaultQuotaResp) {};
      GetDefaultQuotaRespMsg(QuotaDefaultLimits defaultLimits) :
         BaseType(NETMSGTYPE_GetDefaultQuotaResp), limits(defaultLimits) {};
      virtual ~GetDefaultQuotaRespMsg() {};


   private:
      QuotaDefaultLimits limits;


   public:
      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx % obj->limits;
      }

      QuotaDefaultLimits& getDefaultLimits()
      {
         return limits;
      }
};

#endif /* COMMON_NET_MESSAGE_STORAGE_QUOTA_GETDEFAULTQUOTARESPMSG_H_ */
