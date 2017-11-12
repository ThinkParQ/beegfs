#include "FindLinkOwnerRespMsg.h"

bool FindLinkOwnerRespMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;
   
   // result
   
   unsigned resultBufLen;
   
   if(!Serialization::deserializeInt(&buf[bufPos], bufLen-bufPos,
      &result, &resultBufLen) )
      return false;
   
   bufPos += resultBufLen;

   // parentDirID

   unsigned parentDirIDBufLen;

   if(!Serialization::deserializeStr(&buf[bufPos], bufLen-bufPos,
      &parentDirIDLen, &parentDirID, &parentDirIDBufLen) )
      return false;

   bufPos += parentDirIDBufLen;

   // linkOwnerNodeID

   unsigned linkOwnerNodeIDBufLen;

   if(!Serialization::deserializeUShort(&buf[bufPos], bufLen-bufPos,
      &linkOwnerNodeID, &linkOwnerNodeIDBufLen) )
      return false;

   bufPos += linkOwnerNodeIDBufLen;

   return true;   
}

void FindLinkOwnerRespMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;
   
   // result
   bufPos += Serialization::serializeInt(&buf[bufPos], result);

   // parentDirID
   bufPos += Serialization::serializeStr(&buf[bufPos], parentDirIDLen, parentDirID);

   // ownerNodeID
   bufPos += Serialization::serializeUShort(&buf[bufPos], linkOwnerNodeID);
}



