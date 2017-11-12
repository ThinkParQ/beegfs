#ifndef SETQUOTAMSG_H_
#define SETQUOTAMSG_H_


#include <common/net/message/NetMessage.h>
#include <common/storage/quota/QuotaData.h>
#include <common/Common.h>

#define SETQUOTAMSG_MAX_SIZE_FOR_QUOTA_DATA   ( (unsigned)(NETMSG_MAX_PAYLOAD_SIZE - 1024) )
#define SETQUOTAMSG_MAX_ID_COUNT              ( (unsigned) \
   (SETQUOTAMSG_MAX_SIZE_FOR_QUOTA_DATA / sizeof(QuotaData) ) )


class SetQuotaMsg: public NetMessageSerdes<SetQuotaMsg>
{
   public:
      SetQuotaMsg(StoragePoolId storagePoolId) :
         BaseType(NETMSGTYPE_SetQuota), storagePoolId(storagePoolId)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->storagePoolId
            % obj->quotaData;
      }

   protected:
      // only for deserialization
      SetQuotaMsg() : BaseType(NETMSGTYPE_SetQuota) {}

   private:
      StoragePoolId storagePoolId;
      QuotaDataList quotaData;

   public:
      StoragePoolId getStoragePoolId() const
      {
         return storagePoolId;
      }

      const QuotaDataList& getQuotaDataList() const
      {
         return quotaData;
      }

      void insertQuotaLimit(QuotaData limit)
      {
         quotaData.push_back(limit);
      }
};

#endif /* SETQUOTAMSG_H_ */
