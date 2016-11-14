#include "MkDirMsg.h"

TestingEqualsRes MkDirMsg::testingEquals(NetMessage *msg)
{
   TestingEqualsRes testRes = TestingEqualsRes_TRUE;

   if ( msg->getMsgType() != this->getMsgType() )
      testRes = TestingEqualsRes_FALSE;
   else
   {
      MkDirMsg *msgClone = (MkDirMsg*) msg;

      if ( this->getUserID() != msgClone->getUserID() )
         testRes = TestingEqualsRes_FALSE;
      else
      if ( this->getGroupID() != msgClone->getGroupID() )
            testRes = TestingEqualsRes_FALSE;
      else
      if ( this->getMode() != msgClone->getMode() )
         testRes = TestingEqualsRes_FALSE;
      else
      if ( this->getMode() != msgClone->getMode() )
         testRes = TestingEqualsRes_FALSE;
      else
      if (*this->parentEntryInfoPtr != *msgClone->getParentInfo() )
         testRes = TestingEqualsRes_FALSE;
      else
      if ( strcmp(this->getNewDirName(), msgClone->getNewDirName()) != 0 )
         testRes = TestingEqualsRes_FALSE;
      else
      if (*this->preferredNodes != *msgClone->preferredNodes)
         testRes = TestingEqualsRes_FALSE;
   }

   return testRes;
}
