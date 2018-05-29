#ifndef COMMON_ADDSTORAGEPOOLRESPMSG_H_
#define COMMON_ADDSTORAGEPOOLRESPMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/StorageErrors.h>
#include <common/storage/StoragePoolId.h>

class AddStoragePoolRespMsg : public NetMessageSerdes<AddStoragePoolRespMsg>
{
   public:

      /**
       * @param result return code of the add operation
       * @param poolId ID of the newly inserted pool (if result is SUCCESS)
       */
      AddStoragePoolRespMsg(FhgfsOpsErr result, StoragePoolId poolId) :
         BaseType(NETMSGTYPE_AddStoragePoolResp), result(result), poolId(poolId) { }

      /**
       * For deserialization only
       */
      AddStoragePoolRespMsg() : BaseType(NETMSGTYPE_AddStoragePoolResp){ }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx % obj->result
             % obj->poolId;
      }

      FhgfsOpsErr   getResult() const { return result; };
      StoragePoolId getPoolId() const { return poolId; };

   private:
      FhgfsOpsErr result;
      StoragePoolId poolId;
};

#endif /*COMMON_ADDSTORAGEPOOLRESPMSG_H_*/
