#ifndef COMMON_REMOVESTORAGEPOOLMSG_H_
#define COMMON_REMOVESTORAGEPOOLMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/StoragePoolId.h>

class RemoveStoragePoolMsg : public NetMessageSerdes<RemoveStoragePoolMsg>
{
   public:

      /**
       * @param poolId ID of the pool which shall be removed (if result is SUCCESS)
       */
      RemoveStoragePoolMsg(StoragePoolId poolId) :
         BaseType(NETMSGTYPE_RemoveStoragePool), poolId(poolId) { }

      /**
       * For deserialization only
       */
      RemoveStoragePoolMsg() : BaseType(NETMSGTYPE_RemoveStoragePool){ }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx % obj->poolId;
      }

   protected:
      StoragePoolId poolId;
};

#endif /*COMMON_REMOVESTORAGEPOOLMSG_H_*/
