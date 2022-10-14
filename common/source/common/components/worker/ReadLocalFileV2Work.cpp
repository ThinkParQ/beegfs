#include <common/app/log/LogContext.h>
#include <common/app/AbstractApp.h>
#include <common/threading/PThread.h>
#include <common/net/message/NetMessage.h>
#include <common/toolkit/MessagingTk.h>
#include <common/nodes/NodeStoreServers.h>
#include <common/storage/StorageErrors.h>
#include <common/net/message/session/rw/ReadLocalFileV2Msg.h>
#include "ReadLocalFileV2Work.h"


void ReadLocalFileV2Work::process(
   char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen)
{
   TargetMapper* targetMapper = readInfo->targetMapper;
   NodeStoreServers* nodes = readInfo->storageNodes;
   FhgfsOpsErr resolveErr;

   auto node = nodes->referenceNodeByTargetID(targetID, targetMapper, &resolveErr);
   if (unlikely(!node))
   { // unable to resolve targetID
      *nodeResult = -resolveErr;
   }
   else
   { // got node reference => begin communication
      *nodeResult = communicate(*node, bufIn, bufInLen, bufOut, bufOutLen);
   }

   readInfo->counter->incCount();
}

int64_t ReadLocalFileV2Work::communicate(Node& node, char* bufIn, unsigned bufInLen,
   char* bufOut, unsigned bufOutLen)
{
   const char* logContext = "ReadFileV2 Work (communication)";
   NodeConnPool* connPool = node.getConnPool();
   NumNodeID localNodeNumID = readInfo->localNodeNumID;

   AbstractApp* app = PThread::getCurrentThreadApp();
   int connMsgLongTimeout = app->getCommonConfig()->getConnMsgLongTimeout();

   int64_t retVal = -FhgfsOpsErr_COMMUNICATION;
   int64_t numReceivedFileBytes = 0;
   Socket* sock = NULL;

   try
   {
      // connect
      sock = connPool->acquireStreamSocket();

      // prepare and send message
      ReadLocalFileV2Msg readMsg(localNodeNumID, fileHandleID, targetID, pathInfoPtr,
         accessFlags, offset, size);

      if (this->firstWriteDoneForTarget)
         readMsg.addMsgHeaderFeatureFlag(READLOCALFILEMSG_FLAG_SESSION_CHECK);

      unsigned msgLength = readMsg.serializeMessage(bufOut, bufOutLen).second;
      sock->send(bufOut, msgLength, 0);

      // recv length info and file data loop
      // note: we return directly from within this loop if the received header indicates an end of
      //    transmission
      for( ; ; )
      {
         int64_t lengthInfo; // length info in fhgfs host byte order

         sock->recvExactT(&lengthInfo, sizeof(lengthInfo), 0, connMsgLongTimeout);
         lengthInfo = LE_TO_HOST_64(lengthInfo);

         if(lengthInfo <= 0)
         { // end of file data transmission
            if(unlikely(lengthInfo < 0) )
            { // error occurred
               retVal = lengthInfo;
            }
            else
            { // normal end of file data transmission
               retVal = numReceivedFileBytes;
            }

            connPool->releaseStreamSocket(sock);
            return retVal;
         }

         // buffer overflow check
         if(unlikely( (uint64_t)(numReceivedFileBytes + lengthInfo) > this->size) )
         {
            LogContext(logContext).logErr(
               std::string("Received a lengthInfo that would lead to a buffer overflow from ") +
                  node.getID() + ": " + StringTk::int64ToStr(lengthInfo) );

            retVal = -FhgfsOpsErr_INTERNAL;

            break;
         }

         // positive result => node is going to send some file data

         // receive announced dataPart
         sock->recvExactT(&(this->buf)[numReceivedFileBytes], lengthInfo, 0, connMsgLongTimeout);

         numReceivedFileBytes += lengthInfo;

      } // end of recv file data + header loop

   }
   catch(SocketConnectException& e)
   {
      LogContext(logContext).log(2, std::string("Unable to connect to storage server: ") +
         node.getID() );

      retVal = -FhgfsOpsErr_COMMUNICATION;
   }
   catch(SocketException& e)
   {
      LogContext(logContext).logErr(
         std::string("Communication error. SocketException: ") + e.what() );
      LogContext(logContext).log(2, std::string("Sent request: handleID/offset/size: ") +
         fileHandleID + "/" + StringTk::int64ToStr(offset) + "/" + StringTk::int64ToStr(size) );

      retVal = -FhgfsOpsErr_COMMUNICATION;
   }

   // error clean up

   if(sock)
      connPool->invalidateStreamSocket(sock);

   return retVal;
}
