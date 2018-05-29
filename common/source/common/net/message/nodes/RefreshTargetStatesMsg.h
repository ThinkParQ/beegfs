#ifndef REFRESHTARGETSTATESMSG_H_
#define REFRESHTARGETSTATESMSG_H_

#include <common/net/message/AcknowledgeableMsg.h>
#include <common/Common.h>


/**
 * This msg notifies the receiver about a change in the targets states store. It is typically sent
 * by the mgmtd. The receiver should update its target state store
 */
class RefreshTargetStatesMsg : public AcknowledgeableMsgSerdes<RefreshTargetStatesMsg>
{
   public:
      RefreshTargetStatesMsg() : BaseType(NETMSGTYPE_RefreshTargetStates)
      {
      }
};


#endif /* REFRESHTARGETSTATESMSG_H_ */
