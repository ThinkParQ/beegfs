#ifndef GETXATTRMSGEX_H_
#define GETXATTRMSGEX_H_

#include <common/net/message/storage/attribs/GetXAttrMsg.h>

class GetXAttrMsgEx : public GetXAttrMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /*GETXATTRMSGEX_H_*/
