#ifndef META_REFRESHSTORAGEPOOLSMSGEX_H_
#define META_REFRESHSTORAGEPOOLSMSGEX_H_

#include <common/net/message/nodes/storagepools/RefreshStoragePoolsMsg.h>

class RefreshStoragePoolsMsgEx : public RefreshStoragePoolsMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /*META_REFRESHSTORAGEPOOLSMSGEX_H_*/
