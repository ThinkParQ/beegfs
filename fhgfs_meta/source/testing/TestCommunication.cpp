#include "TestCommunication.h"

#include <common/net/message/nodes/HeartbeatRequestMsg.h>
#include <common/net/message/nodes/HeartbeatMsg.h>

#include <program/Program.h>
#include <net/message/NetMessageFactory.h>

TestCommunication::TestCommunication()
   : log("TestCommunication")
{
}

TestCommunication::~TestCommunication()
{
}

void TestCommunication::setUp()
{
}

void TestCommunication::tearDown()
{
}

/*
 * Test the request response mechanism from MessagingTk
 *
 * here, we test this using a simple HeartbeatRequest/Heartbeat communication
 */
void TestCommunication::testRequestResponse()
{
   log.log(Log_DEBUG, "testRequestResponse started");

   HeartbeatRequestMsg hbReqMsg;

   char* respBuf = NULL;
   NetMessage* respMsg = NULL;
   bool commRes = MessagingTk::requestResponse(
         Program::getApp()->getLocalNode(), &hbReqMsg, NETMSGTYPE_Heartbeat, &respBuf, &respMsg);

   // we just need to know if communication worked => do not verify results
   SAFE_DELETE(respMsg);
   SAFE_FREE(respBuf);

   if(!commRes)
      CPPUNIT_FAIL("Request/Response of a Heartbeat Msg failed");

   log.log(Log_DEBUG, "testRequestResponse finished");
}
