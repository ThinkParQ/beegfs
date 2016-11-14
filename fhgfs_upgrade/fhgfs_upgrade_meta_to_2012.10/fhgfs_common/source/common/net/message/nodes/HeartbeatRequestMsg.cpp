#include "HeartbeatRequestMsg.h"

TestingEqualsRes HeartbeatRequestMsg::testingEquals(NetMessage *msg)
{
   TestingEqualsRes testRes = TestingEqualsRes_TRUE;
   if ( msg->getMsgType() != this->getMsgType() )
      testRes = TestingEqualsRes_FALSE;

   return testRes;
}


