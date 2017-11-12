#ifndef GETNODECAPACITYPOOLSRESP_H_
#define GETNODECAPACITYPOOLSRESP_H_

#include <common/Common.h>
#include <common/net/message/NetMessage.h>
#include <common/nodes/Node.h>
#include <common/storage/StoragePoolId.h>

class GetNodeCapacityPoolsRespMsg : public NetMessageSerdes<GetNodeCapacityPoolsRespMsg>
{
   public:
      // a map with storagePoolId as key and a vector of capacity pool lists as value. The
      // vector has CapacityPoolType as index, so vec[CapacityPool_NORMAL] represents th normal
      // pool, etc.
      typedef std::map<StoragePoolId, UInt16ListVector> PoolsMap;

      /**
       * @param capacityLists a map with storagePoolId as key and a vector of capacity pool lists
       *        as value. For metadata pools, the map key is irrelevant and there will be only one
       *        entry in the map with key 0.
       */
      GetNodeCapacityPoolsRespMsg(PoolsMap* poolsMap) :
            BaseType(NETMSGTYPE_GetNodeCapacityPoolsResp), poolsMap(poolsMap)
      { }

      GetNodeCapacityPoolsRespMsg() : BaseType(NETMSGTYPE_GetNodeCapacityPoolsResp)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::backedPtr(obj->poolsMap, obj->parsed.poolsMap);
      }

   private:
      // for serialization
      PoolsMap* poolsMap; // not owned by this object!

      // for deserialization
      struct {
         PoolsMap poolsMap;
      } parsed;

   public:
      PoolsMap& getPoolsMap()
      {
         return *poolsMap;
      }
};

#endif /* GETNODECAPACITYPOOLSRESP_H_ */
