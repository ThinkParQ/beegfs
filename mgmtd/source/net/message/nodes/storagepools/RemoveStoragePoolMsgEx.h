#ifndef MGMTD_REMOVESTORAGEPOOLMSGEX_H_
#define MGMTD_REMOVESTORAGEPOOLMSGEX_H_

#include <common/net/message/nodes/storagepools/RemoveStoragePoolMsg.h>

class RemoveStoragePoolMsgEx : public RemoveStoragePoolMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /*MGMTD_REMOVESTORAGEPOOLMSGEX_H_*/
