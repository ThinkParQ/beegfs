#ifndef RMDIRRESPMSG_H_
#define RMDIRRESPMSG_H_

#include <common/net/message/SimpleIntMsg.h>

class RmDirRespMsg : public SimpleIntMsg
{
   public:
      RmDirRespMsg(int result) : SimpleIntMsg(NETMSGTYPE_RmDirResp, result)
      {
      }

      RmDirRespMsg() : SimpleIntMsg(NETMSGTYPE_RmDirResp)
      {
      }
};

#endif /*RMDIRRESPMSG_H_*/
