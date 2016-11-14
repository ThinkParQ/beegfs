#include "StatStoragePathRespMsg.h"

void StatStoragePathRespMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;
   
   // result
   bufPos += Serialization::serializeInt(&buf[bufPos], result);

   // sizeTotal
   bufPos += Serialization::serializeInt64(&buf[bufPos], sizeTotal);

   // sizeFree
   bufPos += Serialization::serializeInt64(&buf[bufPos], sizeFree);

   // inodesTotal
   bufPos += Serialization::serializeInt64(&buf[bufPos], inodesTotal);

   // inodesFree
   bufPos += Serialization::serializeInt64(&buf[bufPos], inodesFree);
}

bool StatStoragePathRespMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;
   
   // result
   unsigned resultFieldLen;
   if(!Serialization::deserializeInt(&buf[bufPos], bufLen-bufPos, &result, &resultFieldLen) )
      return false;

   bufPos += resultFieldLen;

   // sizeTotal
   unsigned sizeTotalFieldLen;
   if(!Serialization::deserializeInt64(&buf[bufPos], bufLen-bufPos, &sizeTotal, &sizeTotalFieldLen) )
      return false;

   bufPos += sizeTotalFieldLen;

   // sizeFree
   unsigned sizeFreeFieldLen;
   if(!Serialization::deserializeInt64(&buf[bufPos], bufLen-bufPos, &sizeFree, &sizeFreeFieldLen) )
      return false;

   bufPos += sizeFreeFieldLen;
      
   // inodesTotal
   unsigned inodesTotalFieldLen;
   if(!Serialization::deserializeInt64(&buf[bufPos], bufLen-bufPos, &inodesTotal,
      &inodesTotalFieldLen) )
      return false;

   bufPos += inodesTotalFieldLen;

   // inodesFree
   unsigned inodesFreeFieldLen;
   if(!Serialization::deserializeInt64(&buf[bufPos], bufLen-bufPos, &inodesFree,
      &inodesFreeFieldLen) )
      return false;

   bufPos += inodesFreeFieldLen;

   return true;
}


