#ifndef SETSTORAGETARGETINFOMSGEX_H_
#define SETSTORAGETARGETINFOMSGEX_H_

#include <common/net/message/storage/SetStorageTargetInfoMsg.h>

class SetStorageTargetInfoMsgEx : public SetStorageTargetInfoMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /*SETSTORAGETARGETINFOMSGEX_H_*/
