#include "MovingFileInsertRespMsg.h"

void MovingFileInsertRespMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;
   
   // result
   bufPos += Serialization::serializeInt(&buf[bufPos], this->result);
   
   // chunkHash
   bufPos += Serialization::serializeUInt(&buf[bufPos], this->chunkHash);

   // unlinkedFileID
   bufPos += Serialization::serializeStr(&buf[bufPos],
      this->unlinkedFileIDLen, this->unlinkedFileID);

   // stripePattern
   bufPos += pattern->serialize(&buf[bufPos]);
   
}

bool MovingFileInsertRespMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;
   
   { // result
      unsigned resultFieldLen;
      if(!Serialization::deserializeInt(&buf[bufPos], bufLen-bufPos, &this->result,
         &resultFieldLen) )
         return false;

      bufPos += resultFieldLen;
   }
   
   { // chunkHash
      unsigned chunkHashLen;

      if(!Serialization::deserializeUInt(&buf[bufPos], bufLen-bufPos, &this->chunkHash,
         &chunkHashLen) )
         return false;

      bufPos += chunkHashLen;
   }

   { // unlinkedFileID
      unsigned fileIDBufLen;

      if(!Serialization::deserializeStr(&buf[bufPos], bufLen-bufPos,
         &this->unlinkedFileIDLen, &this->unlinkedFileID, &fileIDBufLen) )
         return false;

      bufPos += fileIDBufLen;
   }

   { // stripePattern
      unsigned patternBufLen;
      if(!StripePattern::deserializePatternPreprocess(&buf[bufPos], bufLen-bufPos,
         &this->patternHeader, &this->patternStart, &patternBufLen) )
         return false;
      
      bufPos += patternBufLen;
   }

   return true;
}


