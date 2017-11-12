#ifndef COMMON_GETSTORAGEPOOLSRESPMSG_H_
#define COMMON_GETSTORAGEPOOLSRESPMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/StoragePool.h>

class GetStoragePoolsRespMsg : public NetMessageSerdes<GetStoragePoolsRespMsg>
{
   public:
      /**
       * @param pools a vector of storage pools; just a reference
       */
      GetStoragePoolsRespMsg(StoragePoolPtrVec* pools):
         BaseType(NETMSGTYPE_GetStoragePoolsResp), pools(pools) { }

      /**
       * For deserialization only
       */
      GetStoragePoolsRespMsg():
         BaseType(NETMSGTYPE_GetStoragePoolsResp) { }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::backedPtr(obj->pools, obj->parsed.pools);
      }

      StoragePoolPtrVec& getStoragePools() const
      {
         return *pools;
      }

   private:
      StoragePoolPtrVec* pools; // not owned by this object!

      // for deserialization
      struct
      {
         StoragePoolPtrVec pools;
      } parsed;

};

#endif /*COMMON_GETSTORAGEPOOLSRESPMSG_H_*/
