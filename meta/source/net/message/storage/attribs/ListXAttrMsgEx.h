#ifndef LISTXATTRMSGEX_H_
#define LISTXATTRMSGEX_H_

#include <common/net/message/storage/attribs/ListXAttrMsg.h>

class ListXAttrMsgEx : public ListXAttrMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);

};

#endif /*LISTXATTRMSGEX_H_*/
