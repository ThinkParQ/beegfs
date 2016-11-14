#include "MkLocalFileMsg.h"


bool MkLocalFileMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;
   
   // path
   unsigned pathBufLen;
   if(!Serialization::deserializePathPreprocess(&buf[bufPos], bufLen-bufPos,
      &pathDeserInfo, &pathBufLen) )
      return false;
   
   bufPos += pathBufLen;
   
   // stripePattern
   unsigned patternBufLen;
   if(!StripePattern::deserializePatternPreprocess(&buf[bufPos], bufLen-bufPos,
      &patternHeader, &patternStart, &patternBufLen) )
      return false;
   
   bufPos += patternBufLen; // not needed right now.

   return true;   
}

void MkLocalFileMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;
   
   // path
   
   bufPos += Serialization::serializePath(&buf[bufPos], path);
   
   // stripePattern
   
   bufPos += pattern->serialize(&buf[bufPos]);
}

