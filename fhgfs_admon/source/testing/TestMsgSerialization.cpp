#include "TestMsgSerialization.h"

#include <common/net/message/admon/GetNodeInfoMsg.h>
#include <common/net/message/admon/GetNodeInfoRespMsg.h>
#include <common/net/message/nodes/HeartbeatMsg.h>
#include <net/message/NetMessageFactory.h>

TestMsgSerialization::TestMsgSerialization()
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

void TestMsgSerialization::testGetNodeInfoMsgSerialization()
{
   log.log(Log_DEBUG, "testGetNodeInfoMsgSerialization started");

   GetNodeInfoMsg msg;
   GetNodeInfoMsg msgClone;

   bool testRes = this->testMsgSerialization(msg, msgClone);

   if (!testRes)
      CPPUNIT_FAIL("Failed to serialize/deserialize GetNodeInfoMsg");

   log.log(Log_DEBUG, "testGetNodeInfoMsgSerialization finished");
}

void TestMsgSerialization::testGetNodeInfoRespMsgSerialization()
{
   log.log(Log_DEBUG, "testGetNodeInfoRespMsgSerialization started");

   GeneralNodeInfo nodeInfo;
   std::string nodeID = "nodeID";
   NumNodeID nodeNumID(1);
   nodeInfo.cpuName = "cpuName";
   nodeInfo.cpuSpeed = 1500;
   nodeInfo.cpuCount = 16;
   nodeInfo.memTotal = 16000;
   nodeInfo.memFree = 10000;
   nodeInfo.logFilePath = "/var/log/log.log";

   GetNodeInfoRespMsg msg(nodeID, nodeNumID, nodeInfo);
   GetNodeInfoRespMsg msgClone;

   bool testRes = this->testMsgSerialization(msg, msgClone);

   if (!testRes)
      CPPUNIT_FAIL("Failed to serialize/deserialize GetNodeInfoRespMsg");

   log.log(Log_DEBUG, "testGetNodeInfoRespMsgSerialization finished");
}
