#ifndef ACKMSGEX_H_
#define ACKMSGEX_H_

#include <common/net/message/control/AckMsg.h>

// see class AcknowledgeableMsg (fhgfs_common) for a short description

class AckMsgEx : public AckMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};


#endif /* ACKMSGEX_H_ */
