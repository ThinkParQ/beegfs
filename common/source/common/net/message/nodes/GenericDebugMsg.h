#ifndef GENERICDEBUGMSG_H_
#define GENERICDEBUGMSG_H_

#include <common/net/message/SimpleStringMsg.h>
#include <common/Common.h>


/**
 * This message is generic in the way that it simply takes a space-separated command string as input
 * and the response is also just a string (which will just be printed to the console by fhgfs-ctl).
 * That makes this msg very useful to quickly add new fuctionality for testing or query information
 * for debugging purposes without the overhead of adding a completely new message.
 */
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
