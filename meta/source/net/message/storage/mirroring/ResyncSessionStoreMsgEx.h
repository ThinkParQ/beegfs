#ifndef META_RESYNCSESSIONSTOREMSGEX_H
#define META_RESYNCSESSIONSTOREMSGEX_H

#include <common/net/message/storage/mirroring/ResyncSessionStoreMsg.h>

class ResyncSessionStoreMsgEx : public ResyncSessionStoreMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /* META_RESYNCSESSIONSTOREMSGEX_H */
