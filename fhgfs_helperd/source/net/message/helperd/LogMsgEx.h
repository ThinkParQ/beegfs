#ifndef LOGMSGEX_H_
#define LOGMSGEX_H_

#include <common/net/message/helperd/LogMsg.h>

class LogMsgEx : public LogMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /*LOGMSGEX_H_*/
