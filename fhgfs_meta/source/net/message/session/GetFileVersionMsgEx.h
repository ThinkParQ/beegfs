#ifndef GETFILEVERSIONMSGEX_H
#define GETFILEVERSIONMSGEX_H

#include <common/net/message/session/GetFileVersionMsg.h>
#include <common/net/message/session/GetFileVersionRespMsg.h>
#include <net/message/MirroredMessage.h>

class GetFileVersionMsgEx : public GetFileVersionMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx) override;
};

#endif
