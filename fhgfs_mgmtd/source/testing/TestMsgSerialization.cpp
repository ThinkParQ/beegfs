#include "TestMsgSerialization.h"

#include <common/net/message/nodes/GetTargetStatesRespMsg.h>
#include <common/net/message/nodes/HeartbeatRequestMsg.h>
#include <common/net/message/nodes/HeartbeatMsg.h>
#include <common/net/message/storage/creating/MkDirMsg.h>

TestMsgSerialization::TestMsgSerialization()
   : log("TestMsgSerialization")
{
}

TestMsgSerialization::~TestMsgSerialization()
{
}

void TestMsgSerialization::setUp()
{
}

void TestMsgSerialization::tearDown()
{
}

void TestMsgSerialization::testHeartbeatRequestMsgSerialization()
{
	log.log(Log_DEBUG, "testHeartbeatRequestMsgSerialization started");

	HeartbeatRequestMsg msg;
	HeartbeatRequestMsg msgClone;

	bool testRes = this->testMsgSerialization(msg, msgClone);

	if (!testRes)
	   CPPUNIT_FAIL("Failed to serialize/deserialize HeartbeatRequestMsg");

	log.log(Log_DEBUG, "testHeartbeatRequestMsgSerialization finished");
}

void TestMsgSerialization::testHeartbeatMsgSerialization()
{
	log.log(Log_DEBUG, "testHeartbeatMsgSerialization started");

	NicAddressList nicList;
	BitStore nodeFeatures;

	HeartbeatMsg msg("nodeID", NumNodeID(123), NODETYPE_Client, &nicList, &nodeFeatures);
	msg.setRootNumID(NumNodeID(312) );
	HeartbeatMsg msgClone;

	bool testRes = this->testMsgSerialization(msg, msgClone);

	if (!testRes)
	   CPPUNIT_FAIL("Failed to serialize/deserialize HeartbeatMsg");

	log.log(Log_DEBUG, "testHeartbeatMsgSerialization finished");
}

void TestMsgSerialization::testGetTargetStatesRespMsg()
{
   log.log(Log_DEBUG, "testGetTargetStatesRespMsg started");

   UInt16List targetIDList;
   UInt8List reachabilityStateList;
   UInt8List consistencyStateList;

   for (int i = 0; i < 20; i++)
   {
      targetIDList.push_back(20);
      reachabilityStateList.push_back((i%4));
      consistencyStateList.push_back((i%4));
   }

   GetTargetStatesRespMsg msg(&targetIDList, &reachabilityStateList, &consistencyStateList);
   GetTargetStatesRespMsg msgClone;

   bool testRes = this->testMsgSerialization(msg, msgClone);

   if (!testRes)
      CPPUNIT_FAIL("Failed to serialize/deserialize GetTargetStatesRespMsg");

   log.log(Log_DEBUG, "testGetTargetStatesRespMsg finished");
}
