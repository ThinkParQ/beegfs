#include <common/app/log/LogContext.h>
#include <common/net/message/nodes/HeartbeatRequestMsg.h>
#include <common/net/message/nodes/HeartbeatMsg.h>
#include <common/nodes/NodeStore.h>
#include <common/toolkit/StringTk.h>
#include <common/toolkit/MessagingTk.h>
#include <components/HeartbeatManager.h>
#include <program/Program.h>
#include "IntegrationTests.h"

bool IntegrationTests::perform()
{
   LogContext log("IntegrationTests");
   
   log.log(1, "Running integration tests...");

   bool testRes = 
      testStreamWorkers();
   
   log.log(1, "Integration tests complete");
   
   return testRes;
}

bool IntegrationTests::testStreamWorkers()
{
   bool result = true;
   
   LogContext log("testStreamWorkers");
   
   NodeStoreServers* metaNodes = Program::getApp()->getMetaNodes();
   Node& localNode = Program::getApp()->getLocalNode();
   NumNodeID rootNodeID = metaNodes->getRootNodeNumID();
   NodeConnPool* connPool = localNode.getConnPool();
   NicAddressList nicList(localNode.getNicList() );
   const BitStore* nodeFeatureFlags = localNode.getNodeFeatures();

   // create request and response buf
   HeartbeatRequestMsg hbReqMsg;
   std::pair<char*, unsigned> sendBuf = MessagingTk::createMsgBuf(&hbReqMsg);

   HeartbeatMsg localHBMsg(localNode.getID(), NumNodeID(0), NODETYPE_Client, &nicList,
      nodeFeatureFlags);
   localHBMsg.setRootNumID(rootNodeID);
   std::pair<char*, unsigned> localHBMsgBuf = MessagingTk::createMsgBuf(&localHBMsg);

   boost::scoped_array<char> recvBuf(new char[localHBMsgBuf.second]);

   // send & receive
   Socket* sock = NULL;
   try
   {
      sock = connPool->acquireStreamSocket();

      sock->send(sendBuf.first, sendBuf.second, 0);
      
      log.log(4, "Request sent");

      sock->recvExactT(recvBuf.get(), localHBMsgBuf.second, 0, CONN_MEDIUM_TIMEOUT);

      log.log(4, "Response received");

      log.log(4, "Invalidating connection...");
      
      //connPool->releaseStreamSocket(sock);
      connPool->invalidateStreamSocket(sock);
      
      log.log(4, "Disconnected");

      if(memcmp(localHBMsgBuf.first, recvBuf.get(), localHBMsgBuf.second) )
      {
         log.logErr("Heartbeat comparison failed");
         result = false;
      }
      else
         log.log(3, "Heartbeats OK");
   }
   catch(SocketConnectException& e)
   {
      log.logErr("Unable to connect to localhost");
      result = false;
   }
   catch(SocketException& e)
   {
      log.logErr(std::string("SocketException occurred: ") + e.what() );
      
      connPool->invalidateStreamSocket(sock);
      result = false;
   }
   
   free(sendBuf.first);
   free(localHBMsgBuf.first);

   return result;
}










