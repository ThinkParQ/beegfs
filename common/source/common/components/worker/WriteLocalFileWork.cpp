#include <common/app/log/LogContext.h>
#include <common/app/AbstractApp.h>
#include <common/threading/PThread.h>
#include <common/net/message/NetMessage.h>
#include <common/toolkit/MessagingTk.h>
#include <common/nodes/NodeStoreServers.h>
#include <common/storage/StorageErrors.h>
#include <common/net/message/session/rw/WriteLocalFileMsg.h>
#include <common/net/message/session/rw/WriteLocalFileRespMsg.h>
#include "WriteLocalFileWork.h"

void WriteLocalFileWork::process(
   char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen)
{
   TargetMapper* targetMapper = writeInfo->targetMapper;
   NodeStoreServers* nodes = writeInfo->storageNodes;
   FhgfsOpsErr resolveErr;

   auto node = nodes->referenceNodeByTargetID(targetID, targetMapper, &resolveErr);
   if(unlikely(!node) )
   { // unable to resolve targetID
      *nodeResult = -resolveErr;
   }
   else
   { // got node reference => begin communication
      *nodeResult = communicate(*node, bufOut, bufOutLen);
   }

   writeInfo->counter->incCount();
}

int64_t WriteLocalFileWork::communicate(Node& node, char* bufOut, unsigned bufOutLen)
{
   const char* logContext = "WriteFile Work (communication)";
   AbstractApp* app = PThread::getCurrentThreadApp();
   NodeConnPool* connPool = node.getConnPool();
   NumNodeID localNodeNumID = writeInfo->localNodeNumID;

   int64_t retVal = -FhgfsOpsErr_COMMUNICATION;
   Socket* sock = NULL;

   try
   {
      // connect
      sock = connPool->acquireStreamSocket();

      // prepare and send message
      WriteLocalFileMsg writeMsg(localNodeNumID, fileHandleID, targetID, pathInfo,
         accessFlags, offset, size);

      if (this->firstWriteDoneForTarget)
         writeMsg.addMsgHeaderFeatureFlag(WRITELOCALFILEMSG_FLAG_SESSION_CHECK);

      unsigned msgLength = writeMsg.serializeMessage(bufOut, bufOutLen).second;
      sock->send(bufOut, msgLength, 0);

      writeMsg.sendData(buf, sock);

      // receive response
      auto resp = MessagingTk::recvMsgBuf(*sock);
      if (resp.empty())
      { // error
         LogContext log(logContext);
         log.log(Log_WARNING, "Did not receive a response from: " + sock->getPeername() );
      }
      else
      { // got response => deserialize it
         auto respMsg = app->getNetMessageFactory()->createFromBuf(std::move(resp));

         if(respMsg->getMsgType() != NETMSGTYPE_WriteLocalFileResp)
         { // response invalid (wrong msgType)
            LogContext log(logContext);
            log.logErr(std::string("Received invalid response type: ") +
               StringTk::intToStr(respMsg->getMsgType() ) + std::string(". ") +
               std::string("Expected: ") + StringTk::intToStr(NETMSGTYPE_WriteLocalFileResp) +
               std::string(". ") +
               std::string("Disconnecting: ") + sock->getPeername() );
         }
         else
         { // correct response => return it
            connPool->releaseStreamSocket(sock);

            WriteLocalFileRespMsg* writeRespMsg = (WriteLocalFileRespMsg*)respMsg.get();
            return writeRespMsg->getValue();
         }
      }
   }
   catch(SocketConnectException& e)
   {
      LogContext(logContext).log(Log_WARNING, std::string("Unable to connect to storage node: ") +
         node.getID() );

      retVal = -FhgfsOpsErr_COMMUNICATION;
   }
   catch(SocketException& e)
   {
      LogContext(logContext).logErr(std::string("SocketException: ") + e.what() );
      LogContext(logContext).log(Log_WARNING, "Values for sessionID/handleID/offset/size: " +
         localNodeNumID.str() + "/" + fileHandleID +
         "/" + StringTk::int64ToStr(offset) + "/" + StringTk::int64ToStr(size) );

      retVal = -FhgfsOpsErr_COMMUNICATION;
   }

   // clean up
   if(sock)
      connPool->invalidateStreamSocket(sock);

   return retVal;
}
