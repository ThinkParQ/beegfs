#ifndef RESYNCRAWINODESRESPMSG_H_
#define RESYNCRAWINODESRESPMSG_H_

#include <common/net/message/SimpleIntMsg.h>

class ResyncRawInodesRespMsg : public SimpleIntMsg
{
   public:
      ResyncRawInodesRespMsg(FhgfsOpsErr result)
         : SimpleIntMsg(NETMSGTYPE_ResyncRawInodesResp, result) { }

      ResyncRawInodesRespMsg()
         : SimpleIntMsg(NETMSGTYPE_ResyncRawInodesResp) { }

      FhgfsOpsErr getResult() { return static_cast<FhgfsOpsErr>(getValue()); }
};

#endif
