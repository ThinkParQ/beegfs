#include "TestCommunication.h"

#include <common/net/message/helperd/GetHostByNameMsg.h>
#include <common/net/message/helperd/GetHostByNameRespMsg.h>
#include <common/toolkit/MessagingTk.h>

#include <program/Program.h>

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
 * here, we test this using a GetHostByName/GetHostByNameResp communication
 */
void TestCommunication::testRequestResponse()
{
   log.log(Log_DEBUG, "testRequestResponse started");

   GetHostByNameMsg getHostByNameMsg(Program::getApp()->getLocalNode().getID().c_str());

   char* respBuf = NULL;
   NetMessage* respMsg = NULL;
   bool commRes = MessagingTk::requestResponse(
         Program::getApp()->getLocalNode(), &getHostByNameMsg, NETMSGTYPE_GetHostByNameResp,
         &respBuf, &respMsg);

   // we just need to know if communication worked => do not verify results
   SAFE_DELETE(respMsg);
   SAFE_FREE(respBuf);

   if(! commRes)
      CPPUNIT_FAIL("Request/Response of a GetHostByNameMsg Msg failed");

   log.log(Log_DEBUG, "testRequestResponse finished");
}
