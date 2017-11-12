#ifndef REGISTERTARGETMSGEX_H_
#define REGISTERTARGETMSGEX_H_


#include <common/net/message/nodes/RegisterTargetMsg.h>

class RegisterTargetMsgEx : public RegisterTargetMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /* REGISTERTARGETMSGEX_H_ */
