#ifndef FLOCKAPPENDMSGEX_H_
#define FLOCKAPPENDMSGEX_H_

#include <common/net/message/session/locking/FLockAppendMsg.h>
#include <common/storage/StorageErrors.h>
#include <storage/FileInode.h>


class FLockAppendMsgEx : public FLockAppendMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /* FLOCKAPPENDMSGEX_H_ */
