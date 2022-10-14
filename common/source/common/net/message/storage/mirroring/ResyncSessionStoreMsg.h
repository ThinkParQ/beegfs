#ifndef COMMON_RESYNCSESSIONSTOREMSG_H
#define COMMON_RESYNCSESSIONSTOREMSG_H

#include <common/net/message/NetMessage.h>
#include <common/toolkit/MessagingTk.h>

#include <utility>

class ResyncSessionStoreMsg : public NetMessageSerdes<ResyncSessionStoreMsg>
{
   public:
      typedef FhgfsOpsErr (*GetStoreBufFn)(void* context);

      ResyncSessionStoreMsg(char* sessionStoreBuf, size_t sessionStoreBufSize) :
         BaseType(NETMSGTYPE_ResyncSessionStore),
         sessionStoreBuf(sessionStoreBuf),
         sessionStoreBufSize(sessionStoreBufSize)
      { }

      ResyncSessionStoreMsg() : BaseType(NETMSGTYPE_ResyncSessionStore) { }

   template<typename This, typename Ctx>
   static void serialize(This obj, Ctx& ctx)
   {
      ctx % obj->sessionStoreBufSize;
   }

   private:
      char* sessionStoreBuf;
      size_t sessionStoreBufSize;

      struct {
         std::unique_ptr<char[]> sessionStoreBuf;
      } parsed;

      static FhgfsOpsErr streamToSocketFn(Socket* socket, void* context)
      {
         ResyncSessionStoreMsg* msg = static_cast<ResyncSessionStoreMsg*>(context);
         socket->send(msg->sessionStoreBuf, msg->sessionStoreBufSize, 0);
         return FhgfsOpsErr_SUCCESS;
      }

   public:
      std::pair<char*, size_t> getSessionStoreBuf()
      {
         return std::pair<char*, size_t>(sessionStoreBuf, sessionStoreBufSize);
      }

      FhgfsOpsErr receiveStoreBuf(Socket* socket, int connMsgShortTimeout)
      {
         parsed.sessionStoreBuf.reset(new (std::nothrow)char[sessionStoreBufSize]);
         if (!parsed.sessionStoreBuf)
            return FhgfsOpsErr_OUTOFMEM;

         sessionStoreBuf = parsed.sessionStoreBuf.get();

         ssize_t received = socket->recvExactT(sessionStoreBuf, sessionStoreBufSize,
            0, connMsgShortTimeout);

         if (received < 0 || received < (ssize_t)sessionStoreBufSize)
            return FhgfsOpsErr_COMMUNICATION;

         return FhgfsOpsErr_SUCCESS;
      }

      void registerStreamoutHook(RequestResponseArgs& rrArgs)
      {
         rrArgs.sendExtraData = &streamToSocketFn;
         rrArgs.extraDataContext = this;
      }
};

#endif /* COMMON_RESYNCSESSIONSTOREMSG_H */
