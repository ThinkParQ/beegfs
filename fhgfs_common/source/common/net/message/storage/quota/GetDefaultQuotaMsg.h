#ifndef COMMON_GETDEFAULTQUOTAMSG_H_
#define COMMON_GETDEFAULTQUOTAMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/StoragePoolId.h>

class GetDefaultQuotaMsg : public NetMessageSerdes<GetDefaultQuotaMsg>
{
   public:
      /**
       * @param storagePoolId request information for this storage pool
       */
      GetDefaultQuotaMsg(StoragePoolId storagePoolId):
            BaseType(NETMSGTYPE_GetDefaultQuota), storagePoolId(storagePoolId)
      { }

      /**
       * For deserialization only
       */
      GetDefaultQuotaMsg():
            BaseType(NETMSGTYPE_GetDefaultQuota)
      { }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->storagePoolId;
      }

   protected:
      StoragePoolId storagePoolId;
};

#endif /* COMMON_GETDEFAULTQUOTAMSG_H_ */
