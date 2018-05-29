#ifndef GETSTORAGETARGETINFOMSGEX_H
#define GETSTORAGETARGETINFOMSGEX_H

#include <common/net/message/storage/GetStorageTargetInfoMsg.h>
#include <common/nodes/TargetStateStore.h>


class GetStorageTargetInfoMsgEx : public GetStorageTargetInfoMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};


#endif /*GETSTORAGETARGETINFOMSGEX_H*/
