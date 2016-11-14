#include "HeartbeatMsg.h"

#include <common/toolkit/ZipIterator.h>

TestingEqualsRes HeartbeatMsg::testingEquals(NetMessage *msg)
{
   if ( msg->getMsgType() != this->getMsgType() )
      return TestingEqualsRes_FALSE;

   HeartbeatMsg *hbMsg = (HeartbeatMsg*) msg;

   if (hbMsg->getNodeID() != this->getNodeID())
      return TestingEqualsRes_FALSE;

   if(hbMsg->getNodeNumID() != this->getNodeNumID() )
      return TestingEqualsRes_FALSE;

   if(hbMsg->getRootNumID() != this->getRootNumID() )
      return TestingEqualsRes_FALSE;

   if(hbMsg->getRootIsBuddyMirrored() != this->getRootIsBuddyMirrored() )
      return TestingEqualsRes_FALSE;

   NicAddressList& nicListClone = hbMsg->getNicList();

   if ( nicListClone.size() != this->nicList->size() )
      return TestingEqualsRes_FALSE;

   // run only if sizes do match
   for (ZipIterRange<NicAddressList, NicAddressList> origCloneIter(*this->nicList, nicListClone);
        !origCloneIter.empty(); ++origCloneIter)
   {
      NicAddress nicAddrOrig = *(origCloneIter()->first);
      NicAddress nicAddrClone = *(origCloneIter()->second);

      if ( (nicAddrOrig.ipAddr.s_addr != nicAddrClone.ipAddr.s_addr)
         || (nicAddrOrig.nicType != nicAddrClone.nicType) )
      {
         return TestingEqualsRes_FALSE;
      }
   }

   return TestingEqualsRes_TRUE;
}


