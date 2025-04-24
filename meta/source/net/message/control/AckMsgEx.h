#pragma once

#include <common/net/message/control/AckMsg.h>

// see class AcknowledgeableMsg (fhgfs_common) for a short description

class AckMsgEx : public AckMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};


