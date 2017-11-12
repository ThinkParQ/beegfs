#include "RmDirEntryMsg.h"

bool RmDirEntryMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;

   // path

   unsigned pathBufLen;

   if(!Serialization::deserializePathPreprocess(&buf[bufPos], bufLen-bufPos,
      &pathDeserInfo, &pathBufLen) )
      return false;

   bufPos += pathBufLen; // not needed right now. included to avoid human errors later ;)

   return true;
}

void RmDirEntryMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;

   // path

   bufPos += Serialization::serializePath(&buf[bufPos], path);
}

