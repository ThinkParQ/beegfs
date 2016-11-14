#ifndef FSCKSETEVENTLOGGINGMSG_H
#define FSCKSETEVENTLOGGINGMSG_H

#include <common/net/message/NetMessage.h>
#include <common/toolkit/ListTk.h>
#include <common/toolkit/serialization/Serialization.h>

class FsckSetEventLoggingMsg: public NetMessageSerdes<FsckSetEventLoggingMsg>
{
   public:
      FsckSetEventLoggingMsg(bool enableLogging, unsigned portUDP, NicAddressList* nicList,
            bool forceRestart) :
         BaseType(NETMSGTYPE_FsckSetEventLogging),
         enableLogging(enableLogging),
         portUDP(portUDP),
         nicList(nicList),
         forceRestart(forceRestart)
      { }

      FsckSetEventLoggingMsg() : BaseType(NETMSGTYPE_FsckSetEventLogging)
      { }

   private:
      bool enableLogging;
      uint32_t portUDP;
      NicAddressList* nicList;
      bool forceRestart;

      // for (de)-serialization
      struct {
         NicAddressList nicList;
      } parsed;

   public:
      bool getEnableLogging() const { return this->enableLogging; }
      unsigned getPortUDP() const { return this->portUDP; }
      NicAddressList& getNicList() { return *nicList; }
      bool getForceRestart() const { return forceRestart; }

      // inliner
      virtual TestingEqualsRes testingEquals(NetMessage* msg)
      {
         FsckSetEventLoggingMsg* msgIn = (FsckSetEventLoggingMsg*) msg;

         if ( this->enableLogging != msgIn->getEnableLogging())
            return TestingEqualsRes_FALSE;

         if ( this->portUDP != msgIn->getPortUDP())
            return TestingEqualsRes_FALSE;

         if (this->forceRestart != msgIn->getForceRestart())
            return TestingEqualsRes_FALSE;

         return TestingEqualsRes_TRUE;
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->enableLogging
            % obj->portUDP
            % serdes::backedPtr(obj->nicList, obj->parsed.nicList)
            % obj->forceRestart;
      }
};

#endif /*FSCKSETEVENTLOGGINGMSG_H*/
