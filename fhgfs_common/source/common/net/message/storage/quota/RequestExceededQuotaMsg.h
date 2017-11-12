#ifndef REQUESTEXCEEDEDQUOTAMSG_H_
#define REQUESTEXCEEDEDQUOTAMSG_H_

#include <common/Common.h>
#include <common/net/message/NetMessage.h>
#include <common/nodes/StoragePoolStore.h>
#include <common/storage/quota/QuotaData.h>
#include <common/storage/StoragePoolId.h>

class RequestExceededQuotaMsg: public NetMessageSerdes<RequestExceededQuotaMsg>
{
   public:
      RequestExceededQuotaMsg(QuotaDataType idType, QuotaLimitType exType,
         StoragePoolId storagePoolId) :
         BaseType(NETMSGTYPE_RequestExceededQuota),
         quotaDataType(idType),
         exceededType(exType),
         storagePoolId(storagePoolId),
         targetId(0) { }

      RequestExceededQuotaMsg(QuotaDataType idType, QuotaLimitType exType, uint16_t targetId) :
         BaseType(NETMSGTYPE_RequestExceededQuota),
         quotaDataType(idType),
         exceededType(exType),
         storagePoolId(StoragePoolStore::INVALID_POOL_ID),
         targetId(targetId) { }

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
            % obj->exceededType
            % obj->storagePoolId
            % obj->targetId;
      }

   private:
      int32_t quotaDataType;
      int32_t exceededType;
      StoragePoolId storagePoolId;
      uint16_t targetId;

   public:
      // getters and setters
      QuotaDataType getQuotaDataType() const
      {
         return (QuotaDataType) quotaDataType;
      }

      QuotaLimitType getExceededType() const
      {
         return (QuotaLimitType) exceededType;
      }

      StoragePoolId getStoragePoolId() const
      {
         return storagePoolId;
      }

      uint16_t getTargetId() const
      {
         return targetId;
      }
};

#endif /* REQUESTEXCEEDEDQUOTAMSG_H_ */
