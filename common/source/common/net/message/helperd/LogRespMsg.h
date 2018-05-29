#ifndef LOGRESPMSG_H_
#define LOGRESPMSG_H_

#include <common/net/message/SimpleIntMsg.h>

class LogRespMsg : public SimpleIntMsg
{
   public:
      LogRespMsg(int result) : SimpleIntMsg(NETMSGTYPE_LogResp, result)
      {
      }

      LogRespMsg() : SimpleIntMsg(NETMSGTYPE_LogResp)
      {
      }
};

#endif /*LOGRESPMSG_H_*/
