#ifndef GENERICDEBUGMSG_H_
#define GENERICDEBUGMSG_H_

#include <common/net/message/SimpleStringMsg.h>


class GenericDebugMsg : public SimpleStringMsg
{
   public:
      GenericDebugMsg(const char* commandStr) : SimpleStringMsg(NETMSGTYPE_GenericDebug, commandStr)
      {
      }

      GenericDebugMsg() : SimpleStringMsg(NETMSGTYPE_GenericDebug)
      {
      }

   private:


   public:
      // getters & setters
      const char* getCommandStr()
      {
         return getValue();
      }
};


#endif /* GENERICDEBUGMSG_H_ */
