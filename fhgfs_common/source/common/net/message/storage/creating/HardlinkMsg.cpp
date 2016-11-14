#include "HardlinkMsg.h"

/**
 * Compare cloneMsg (serialized, then deserialized) with our values
 */
TestingEqualsRes HardlinkMsg::testingEquals(NetMessage* cloneMsg)
{
   HardlinkMsg* cloneLinkMsg = (HardlinkMsg*) cloneMsg;

   if (*this->fromInfoPtr != *cloneLinkMsg->getFromInfo() )
      return TestingEqualsRes_FALSE;

   if (*this->toDirInfoPtr != *cloneLinkMsg->getToDirInfo() )
      return TestingEqualsRes_FALSE;

   if (this->toName != cloneLinkMsg->getToName())
      return TestingEqualsRes_FALSE;

   if (*this->fromDirInfoPtr != *cloneLinkMsg->getFromDirInfo() )
      return TestingEqualsRes_FALSE;

   if (this->fromName != cloneLinkMsg->getFromName() )
      return TestingEqualsRes_FALSE;

   return TestingEqualsRes_TRUE;
}
