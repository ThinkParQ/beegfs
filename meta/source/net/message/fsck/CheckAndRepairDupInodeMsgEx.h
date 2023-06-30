#ifndef CHECKANDREPAIRDUPINODEMSGEX_H
#define CHECKANDREPAIRDUPINODEMSGEX_H

#include <common/net/message/fsck/CheckAndRepairDupInodeMsg.h>

class CheckAndRepairDupInodeMsgEx : public CheckAndRepairDupInodeMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /* CHECKANDREPAIRDUPINODEMSGEX_H */
