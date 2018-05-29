#ifndef COMMON_REFRESHSTORAGEPOOLSMSG_H_
#define COMMON_REFRESHSTORAGEPOOLSMSG_H_

#include <common/net/message/AcknowledgeableMsg.h>

class RefreshStoragePoolsMsg : public AcknowledgeableMsgSerdes<RefreshStoragePoolsMsg>
{
   public:
      /**
       * @param name a describing name for the pool
       * @param targets a vector of target IDs belonging to the new pool; may not be NULL!
       *        just a reference => do not free while you're using this object
       */
      RefreshStoragePoolsMsg():
         BaseType(NETMSGTYPE_RefreshStoragePools) { }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         obj->serializeAckID(ctx);
      }
};

#endif /*COMMON_REFRESHSTORAGEPOOLSMSG_H_*/
