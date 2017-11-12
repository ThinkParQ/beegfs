#ifndef MKLOCALDIRRESPMSG_H_
#define MKLOCALDIRRESPMSG_H_

#include <common/net/message/SimpleIntMsg.h>

class MkLocalDirRespMsg : public SimpleIntMsg
{
   public:
      MkLocalDirRespMsg(int result) : SimpleIntMsg(NETMSGTYPE_MkLocalDirResp, result)
      {
      }

      MkLocalDirRespMsg() : SimpleIntMsg(NETMSGTYPE_MkLocalDirResp)
      {
      }
};

#endif /*MKLOCALDIRRESPMSG_H_*/
