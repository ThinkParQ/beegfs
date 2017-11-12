#ifndef RMLOCALDIRRESPMSG_H_
#define RMLOCALDIRRESPMSG_H_

#include <common/net/message/SimpleIntMsg.h>

class RmLocalDirRespMsg : public SimpleIntMsg
{
   public:
      RmLocalDirRespMsg(int result) : SimpleIntMsg(NETMSGTYPE_RmLocalDirResp, result)
      {
      }

      RmLocalDirRespMsg() : SimpleIntMsg(NETMSGTYPE_RmLocalDirResp)
      {
      }
};

#endif /*RMLOCALDIRRESPMSG_H_*/
