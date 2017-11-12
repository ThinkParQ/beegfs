#include "GetHighResStatsRespMsg.h"

bool GetHighResStatsRespMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;

   // statsList

   unsigned statsListBufLen;

   if(!Serialization::deserializeHighResStatsListPreprocess(&buf[bufPos], bufLen-bufPos,
      &statsListElemNum, &statsListStart, &statsListBufLen) )
      return false;

   bufPos += statsListBufLen;

   return true;
}

void GetHighResStatsRespMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;

   // statsList
   bufPos += Serialization::serializeHighResStatsList(&buf[bufPos], statsList);
}




