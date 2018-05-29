#ifndef UPDATEBACKLINKMSGEX_H
#define UPDATEBACKLINKMSGEX_H

#include <common/net/message/storage/attribs/UpdateBacklinkMsg.h>

class UpdateBacklinkMsgEx : public UpdateBacklinkMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};


#endif /*UPDATEBACKLINKMSGEX_H*/
