#ifndef COMMON_ADDSTORAGEPOOLMSG_H_
#define COMMON_ADDSTORAGEPOOLMSG_H_

#include <common/storage/StoragePoolId.h>
#include <common/net/message/NetMessage.h>

class AddStoragePoolMsg : public NetMessageSerdes<AddStoragePoolMsg>
{
   public:
      /**
       * @param poolId ID for the new pool; may be INVALID, in which case it will be auto-generated
       * @param name a describing name for the pool
       * @param targets a set of target IDs belonging to the new pool; may not be NULL!
       *        just a reference => do not free while you're using this object
       * @param targets a set of buddyGroup IDs belonging to the new pool; may not be NULL!
       *        just a reference => do not free while you're using this object
       */

      AddStoragePoolMsg(StoragePoolId poolId, const std::string& description, 
                        const UInt16Set* targets, const UInt16Set* buddyGroups):
            BaseType(NETMSGTYPE_AddStoragePool), poolId(poolId), description(description), 
            targetsPtr(targets), buddyGroupsPtr(buddyGroups) { }

      /**
       * For deserialization only
       */
      AddStoragePoolMsg():
         BaseType(NETMSGTYPE_AddStoragePool)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->poolId
            % obj->description
            % serdes::backedPtr(obj->targetsPtr, obj->targets)
            % serdes::backedPtr(obj->buddyGroupsPtr, obj->buddyGroups);
      }

   protected:
      StoragePoolId poolId;
      std::string description;

      // for serialization
      const UInt16Set* targetsPtr; // not owned by this object
      // for deserialization
      UInt16Set targets;

      // for serialization
      const UInt16Set* buddyGroupsPtr; // not owned by this object
      // for deserialization
      UInt16Set buddyGroups;
};


#endif /*COMMON_ADDSTORAGEPOOLMSG_H_*/
