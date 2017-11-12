#include "StatRespMsg.h"

void StatRespMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;
   
   // result
   bufPos += Serialization::serializeInt(&buf[bufPos], result);

   // statData
   bufPos += this->statData.serialize(false, &buf[bufPos]);

}

bool StatRespMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;
   
   { // result
      unsigned resultFieldLen;
      if(!Serialization::deserializeInt(&buf[bufPos], bufLen-bufPos, &result, &resultFieldLen) )
         return false;

      bufPos += resultFieldLen;
   }


   { // statData
      unsigned statLen;

      if(!this->statData.deserialize(false, &buf[bufPos], bufLen-bufPos, &statLen) )
         return false;

      bufPos += statLen;
   }

   return true;
}


