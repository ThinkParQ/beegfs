#ifndef MGMTD_MODIFYSTORAGEPOOLMSGEX_H_
#define MGMTD_MODIFYSTORAGEPOOLMSGEX_H_

#include <common/net/message/nodes/storagepools/ModifyStoragePoolMsg.h>

class ModifyStoragePoolMsgEx : public ModifyStoragePoolMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /*MGMTD_MODIFYSTORAGEPOOLMSGEX_H_*/
