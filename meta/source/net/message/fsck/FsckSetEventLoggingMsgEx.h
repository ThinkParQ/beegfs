#ifndef FSCKSETEVENTLOGGINGMSGEX_H
#define FSCKSETEVENTLOGGINGMSGEX_H

#include <common/net/message/fsck/FsckSetEventLoggingMsg.h>
#include <common/net/message/fsck/FsckSetEventLoggingRespMsg.h>

class FsckSetEventLoggingMsgEx : public FsckSetEventLoggingMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /*FSCKSETEVENTLOGGINGMSGEX_H*/
