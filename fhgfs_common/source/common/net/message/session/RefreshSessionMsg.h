#ifndef REFRESHSESSIONMSG_H_
#define REFRESHSESSIONMSG_H_

#include <common/net/message/NetMessage.h>


class RefreshSessionMsg : public NetMessageSerdes<RefreshSessionMsg>
{
   public:

      /**
       * @param sessionID just a reference, so do not free it as long as you use this object!
       */
      RefreshSessionMsg(const char* sessionID) :
         BaseType(NETMSGTYPE_RefreshSession)
      {
         this->sessionID = sessionID;
         this->sessionIDLen = strlen(sessionID);
      }

      RefreshSessionMsg() : BaseType(NETMSGTYPE_RefreshSession)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx % serdes::rawString(obj->sessionID, obj->sessionIDLen);
      }

   private:
      unsigned sessionIDLen;
      const char* sessionID;

   public:

      // getters & setters
      const char* getSessionID() const
      {
         return sessionID;
      }

};

#endif /* REFRESHSESSIONMSG_H_ */
