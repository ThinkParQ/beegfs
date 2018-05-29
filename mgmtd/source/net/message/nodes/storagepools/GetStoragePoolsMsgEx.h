#ifndef GETSTORAGEPOOLSMSGEX_H_
#define GETSTORAGEPOOLSMSGEX_H_

#include <common/net/message/nodes/storagepools/GetStoragePoolsMsg.h>

class GetStoragePoolsMsgEx : public GetStoragePoolsMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /*GETSTORAGEPOOLSMSGEX_H_*/
