#pragma once

#include <common/net/message/AcknowledgeableMsg.h>
#include <common/Common.h>


/**
 * This msg notifies the receiver about a change in the capacity pool lists. It is typically sent by
 * the mgmtd to a mds. The receiver should update its capacity pools.
 */
class RefreshCapacityPoolsMsg : public AcknowledgeableMsgSerdes<RefreshCapacityPoolsMsg>
{
   public:
      /**
       * Default constructor for serialization and deserialization.
       */
      RefreshCapacityPoolsMsg() : BaseType(NETMSGTYPE_RefreshCapacityPools)
      {
      }
};


