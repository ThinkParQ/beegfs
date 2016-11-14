#include "TestCommunication.h"

#include <common/net/message/control/DummyMsg.h>
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
 * here, we test this using a DummyMsg/DummyMsg communication
 */
void TestCommunication::testRequestResponse()
{
   log.log(Log_DEBUG, "testRequestResponse started");

   DummyMsg dummyMsg;

   char* respBuf = NULL;
   NetMessage* respMsg = NULL;
   bool commRes = MessagingTk::requestResponse(
         Program::getApp()->getLocalNode(), &dummyMsg, NETMSGTYPE_Dummy, &respBuf,
         &respMsg);

   // we just need to know if communication worked => do not verify results
   SAFE_DELETE(respMsg);
   SAFE_FREE(respBuf);

   if(! commRes)
      CPPUNIT_FAIL("Request/Response of a DummyMsg failed");

   log.log(Log_DEBUG, "testRequestResponse finished");
}
