#ifndef MGMTD_ADDSTORAGEPOOLMSGEX_H_
#define MGMTD_ADDSTORAGEPOOLMSGEX_H_

#include <common/net/message/nodes/storagepools/AddStoragePoolMsg.h>

class AddStoragePoolMsgEx : public AddStoragePoolMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /*MGMTD_ADDSTORAGEPOOLMSGEX_H_*/
