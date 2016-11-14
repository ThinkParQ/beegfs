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
   
   Node* node = nodes->referenceNodeByTargetID(targetID, targetMapper, &resolveErr);
   if(unlikely(!node) )
   { // unable to resolve targetID
      *nodeResult = -resolveErr;
   }
   else
   { // got node reference => begin communication
      *nodeResult = communicate(node, bufIn, bufInLen, bufOut, bufOutLen);
      nodes->releaseNode(&node);
   }
   
   writeInfo->counter->incCount();
}

int64_t WriteLocalFileWork::communicate(Node* node, char* bufIn, unsigned bufInLen,
   char* bufOut, unsigned bufOutLen)
{
   const char* logContext = "WriteFile Work (communication)";
   AbstractApp* app = PThread::getCurrentThreadApp();
   NodeConnPool* connPool = node->getConnPool();
   const char* localNodeID = writeInfo->localNodeID;
      
   int64_t retVal = -FhgfsOpsErr_COMMUNICATION;
   Socket* sock = NULL;
   NetMessage* respMsg = NULL;
   
   try
   {
      // connect
      sock = connPool->acquireStreamSocket();

      // prepare and send message
      WriteLocalFileMsg writeMsg(localNodeID, fileHandleID, targetID,
         accessFlags, offset, size);
      writeMsg.serialize(bufOut, bufOutLen);
      sock->send(bufOut, writeMsg.getMsgLength(), 0);

      writeMsg.sendData(buf, sock);

      // receive response
      unsigned respLength = MessagingTk::recvMsgBuf(sock, bufIn, bufInLen);
      if(!respLength)
      { // error
         LogContext log(logContext);
         log.log(Log_WARNING, "Did not receive a response from: " + sock->getPeername() );
      }
      else
      { // got response => deserialize it
         respMsg = app->getNetMessageFactory()->createFromBuf(bufIn, respLength);
         
         if(respMsg->getMsgType() != NETMSGTYPE_WriteLocalFileResp)
         { // response invalid (wrong msgType)
            LogContext log(logContext);
            log.log(Log_WARNING, std::string("Received invalid response type: ") +
               StringTk::intToStr(respMsg->getMsgType() ) + std::string(". ") +
               std::string("Expected: ") + StringTk::intToStr(NETMSGTYPE_WriteLocalFileResp) +
               std::string(". ") +
               std::string("Disconnecting: ") + sock->getPeername() );
         }
         else
         { // correct response => return it
            connPool->releaseStreamSocket(sock);
            
            WriteLocalFileRespMsg* writeRespMsg = (WriteLocalFileRespMsg*)respMsg;
            long long writeRespValue = writeRespMsg->getValue();

            SAFE_DELETE(respMsg);
            
            return writeRespValue;
         }
      }
   }
   catch(SocketConnectException& e)
   {
      LogContext(logContext).log(Log_WARNING, std::string("Unable to connect to storage node: ") +
         node->getID() );

      retVal = -FhgfsOpsErr_COMMUNICATION;
   }
   catch(SocketException& e)
   {
      LogContext(logContext).logErr(std::string("SocketException: ") + e.what() );
      LogContext(logContext).log(Log_WARNING, "Values for sessionID/handleID/offset/size: " +
         std::string(localNodeID) + "/" + fileHandleID +
         "/" + StringTk::int64ToStr(offset) + "/" + StringTk::int64ToStr(size) );

      retVal = -FhgfsOpsErr_COMMUNICATION;
   }
   
   // clean up
   if(sock)
      connPool->invalidateStreamSocket(sock);
   SAFE_DELETE(respMsg);

   return retVal;
}
