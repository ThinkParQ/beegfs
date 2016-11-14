#include "TestMsgSerializationBase.h"

#include <common/net/message/nodes/HeartbeatRequestMsg.h>
#include <common/net/message/nodes/HeartbeatMsg.h>
#include <common/net/message/AbstractNetMessageFactory.h>
#include <common/toolkit/MessagingTk.h>

TestMsgSerializationBase::TestMsgSerializationBase()
{
}

TestMsgSerializationBase::~TestMsgSerializationBase()
{
}

void TestMsgSerializationBase::setUp()
{
}

void TestMsgSerializationBase::tearDown()
{
}

bool TestMsgSerializationBase::testMsgSerialization(NetMessage &msg, NetMessage &msgClone)
{
   const char* logContext = "testMsgSerialization";

   bool retVal = true;

   char* msgPayloadBuf = NULL;
   size_t msgPayloadBufLen = 0;

   TestingEqualsRes testRes = TestingEqualsRes_UNDEFINED;
   bool checkCompatRes = false;
   bool deserRes = false;

   // create buffer and serialize message into it
   std::pair<char*, unsigned> buf = MessagingTk::createMsgBuf(&msg);

   // get serialized msg header from buf and check if msgType matches
   NetMessageHeader headerOut;
   NetMessage::deserializeHeader(buf.first, buf.second, &headerOut);

   if (headerOut.msgType != msg.getMsgType())
   {
      retVal = false;
      goto cleanup_and_exit;
   }

   // apply message feature flags and check compatibility

   msgClone.setMsgHeaderFeatureFlags(headerOut.msgFeatureFlags);

   checkCompatRes = msgClone.checkHeaderFeatureFlagsCompat();

   if(!checkCompatRes)
   {
      retVal = false;
      goto cleanup_and_exit;
   }

   // deserialize msg from buf
   msgPayloadBuf = buf.first + NETMSG_HEADER_LENGTH;
   msgPayloadBufLen = buf.second - NETMSG_HEADER_LENGTH;

   deserRes = msgClone.deserializePayload(msgPayloadBuf, msgPayloadBufLen);

   if (!deserRes)
   {
      retVal = false;
      goto cleanup_and_exit;
   }

   testRes = msg.testingEquals(&msgClone);

   if (testRes == TestingEqualsRes_UNDEFINED)
   {
      LogContext(logContext).log(Log_WARNING, "TestingEqual is UNDEFINED; MsgType: " +
         StringTk::intToStr(msg.getMsgType() ) );
   }
   else
   if (testRes == TestingEqualsRes_FALSE)
      retVal = false;

cleanup_and_exit:
   SAFE_FREE(buf.first);

   return retVal;
}
