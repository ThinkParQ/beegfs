#ifndef HARDLINKRESPMSG_H_
#define HARDLINKRESPMSG_H_

#include <common/net/message/SimpleIntMsg.h>

class HardlinkRespMsg : public SimpleIntMsg
{
   public:
      HardlinkRespMsg(int result) : SimpleIntMsg(NETMSGTYPE_HardlinkResp, result)
      {
      }

      HardlinkRespMsg() : SimpleIntMsg(NETMSGTYPE_HardlinkResp)
      {
      }

      // getters & setters
      FhgfsOpsErr getResult()
      {
         return (FhgfsOpsErr)getValue();
      }
};


#endif /*HARDLINKRESPMSG_H_*/
