#ifndef BUMPFILEVERSIONRESPMSG_H
#define BUMPFILEVERSIONRESPMSG_H

#include "../SimpleIntMsg.h"

class BumpFileVersionRespMsg : public SimpleIntMsg
{
   public:
      BumpFileVersionRespMsg(FhgfsOpsErr result) :
         SimpleIntMsg(NETMSGTYPE_BumpFileVersionResp, result)
      {
      }

      BumpFileVersionRespMsg() : SimpleIntMsg(NETMSGTYPE_BumpFileVersionResp)
      {
      }
};


#endif
