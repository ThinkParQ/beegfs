#ifndef MESSAGINGTK_H_
#define MESSAGINGTK_H_

#include <common/app/AbstractApp.h>
#include <common/app/log/LogContext.h>
#include <common/net/message/NetMessage.h>
#include <common/net/sock/Socket.h>
#include <common/net/message/AbstractNetMessageFactory.h>
#include <common/nodes/NodeStoreServers.h>
#include <common/nodes/MirrorBuddyGroupMapper.h>
#include <common/nodes/Node.h>
#include <common/threading/PThread.h>
#include <common/toolkit/MessagingTkArgs.h>
#include <common/Common.h>


#define MESSAGINGTK_INFINITE_RETRY_WAIT_MS (5000) // how long to wait if peer asks for retry


class MessagingTk
{
   public:
      static bool requestResponse(RequestResponseArgs* rrArgs);
      static bool requestResponse(Node& node, NetMessage* requestMsg, unsigned respMsgType,
         char** outRespBuf, NetMessage** outRespMsg);
      static FhgfsOpsErr requestResponseNode(RequestResponseNode* rrNode,
         RequestResponseArgs* rrArgs);
      static FhgfsOpsErr requestResponseTarget(RequestResponseTarget* rrTarget,
         RequestResponseArgs* rrArgs);

      static unsigned recvMsgBuf(Socket* sock, char** outBuf, int minTimeout = 0);
      static unsigned recvMsgBuf(Socket* sock, char* bufIn, size_t bufInLen);
      static std::pair<char*, unsigned> createMsgBuf(NetMessage* msg);

   private:
      MessagingTk() {}

      static FhgfsOpsErr requestResponseComm(RequestResponseArgs* rrArgs);
      static FhgfsOpsErr handleGenericResponse(RequestResponseArgs* rrArgs);
};

#endif /*MESSAGINGTK_H_*/
