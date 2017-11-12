#ifndef MOVINGDIRINSERTRESPMSG_H_
#define MOVINGDIRINSERTRESPMSG_H_

#include <common/net/message/SimpleIntMsg.h>

class MovingDirInsertRespMsg : public SimpleIntMsg
{
   public:
      MovingDirInsertRespMsg(int result) : SimpleIntMsg(NETMSGTYPE_MovingDirInsertResp, result)
      {
      }

      MovingDirInsertRespMsg() : SimpleIntMsg(NETMSGTYPE_MovingDirInsertResp)
      {
      }
};


#endif /*MOVINGDIRINSERTRESPMSG_H_*/
