#include "MkDirMsg.h"


bool MkDirMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;
   
   { // userID
      unsigned userIDLen;
      if(!Serialization::deserializeUInt(&buf[bufPos], bufLen-bufPos, &userID, &userIDLen) )
         return false;

      bufPos += userIDLen;
   }
   
   { // groupID
      unsigned groupIDLen;
      if(!Serialization::deserializeUInt(&buf[bufPos], bufLen-bufPos, &groupID, &groupIDLen) )
         return false;

      bufPos += groupIDLen;
   }

   { // mode
      unsigned modeLen;
      if(!Serialization::deserializeInt(&buf[bufPos], bufLen-bufPos, &mode, &modeLen) )
         return false;

      bufPos += modeLen;
   }

   { // parentEntryInfo
      unsigned parentBufLen;
      this->parentEntryInfo.deserialize(&buf[bufPos], bufLen-bufPos, &parentBufLen);
   
      bufPos += parentBufLen;
   }

   { // newDirName
      unsigned nameLen;
      if(!Serialization::deserializeStrAlign4(&buf[bufPos], bufLen-bufPos,
         &this->newDirNameLen, &this->newDirName, &nameLen) )
         return false;

      bufPos += nameLen;
   }

   { // preferredNodes
      if(!Serialization::deserializeUInt16ListPreprocess(&buf[bufPos], bufLen-bufPos,
         &prefNodesElemNum, &prefNodesListStart, &prefNodesBufLen) )
         return false;

      bufPos += prefNodesBufLen;
   }

   return true;
}

void MkDirMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;
   
   // userID
   
   bufPos += Serialization::serializeUInt(&buf[bufPos], userID);
   
   // groupID
   
   bufPos += Serialization::serializeUInt(&buf[bufPos], groupID);

   // mode
   
   bufPos += Serialization::serializeInt(&buf[bufPos], mode);

   // parentEntryInfo

   bufPos += this->parentEntryInfoPtr->serialize(&buf[bufPos]);

   // newDirName

   bufPos += Serialization::serializeStrAlign4(&buf[bufPos], this->newDirNameLen, this->newDirName);

   // preferredNodes

   bufPos += Serialization::serializeUInt16List(&buf[bufPos], preferredNodes);
}

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
      if (this->getParentInfo() != msgClone->getParentInfo())
		 testRes = TestingEqualsRes_FALSE;
	  else
      if ( strcmp(this->getNewDirName(), msgClone->getNewDirName()) == 0 )
    	 testRes = TestingEqualsRes_FALSE;
	  else
	  {
		 UInt16List clonedPrefferedNodes;
		 msgClone->parsePreferredNodes(&clonedPrefferedNodes);
		 if ( *(this->preferredNodes) != clonedPrefferedNodes )
			testRes = TestingEqualsRes_FALSE;
	  }
   }

   return testRes;
}
