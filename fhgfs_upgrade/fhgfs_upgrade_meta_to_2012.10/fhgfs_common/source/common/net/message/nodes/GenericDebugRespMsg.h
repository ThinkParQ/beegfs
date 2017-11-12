#ifndef GENERICDEBUGRESPMSG_H_
#define GENERICDEBUGRESPMSG_H_

#include <common/net/message/SimpleStringMsg.h>


class GenericDebugRespMsg : public SimpleStringMsg
{
   public:
      GenericDebugRespMsg(const char* cmdRespStr) :
         SimpleStringMsg(NETMSGTYPE_GenericDebugResp, cmdRespStr)
      {
      }

      GenericDebugRespMsg() : SimpleStringMsg(NETMSGTYPE_GenericDebugResp)
      {
      }


   private:


   public:
      // getters & setters
      const char* getCmdRespStr()
      {
         return getValue();
      }

};


#endif /* GENERICDEBUGRESPMSG_H_ */
