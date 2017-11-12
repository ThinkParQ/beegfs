#include <common/app/log/LogContext.h>
#include "ListDirFromOffsetRespMsg.h"

void ListDirFromOffsetRespMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;
   
   // newServerOffset
   bufPos += Serialization::serializeInt64(&buf[bufPos], newServerOffset);

   // result
   bufPos += Serialization::serializeInt(&buf[bufPos], result);

   // entryTypes
   bufPos += Serialization::serializeIntList(&buf[bufPos], entryTypes);

   // names
   bufPos += Serialization::serializeStringList(&buf[bufPos], names);
}

bool ListDirFromOffsetRespMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;
   
   // newServerOffset
   unsigned newServerOffsetBufLen;

   if(!Serialization::deserializeInt64(&buf[bufPos], bufLen-bufPos,
      &newServerOffset, &newServerOffsetBufLen) )
      return false;

   bufPos += newServerOffsetBufLen;

   // result
   unsigned resultFieldLen;
   if(!Serialization::deserializeInt(&buf[bufPos], bufLen-bufPos, &result, &resultFieldLen) )
      return false;

   bufPos += resultFieldLen;
   
   // entryTypes
   if(!Serialization::deserializeIntListPreprocess(&buf[bufPos], bufLen-bufPos,
      &entryTypesElemNum, &entryTypesListStart, &entryTypesBufLen) )
      return false;

   bufPos += entryTypesBufLen;

   // names
   if(!Serialization::deserializeStringListPreprocess(&buf[bufPos], bufLen-bufPos,
      &namesElemNum, &namesListStart, &namesBufLen) )
      return false;
      
   bufPos += namesBufLen;

   if (unlikely(this->entryTypesElemNum != this->namesElemNum) ) // check for equal list lengths
   {
	   LogContext(__func__).log(Log_WARNING, "Sanity check failed: "
			   "entryTypesElemNum != namesElemNum (" + StringTk::intToStr(this->entryTypesElemNum) +
			   " ; " + StringTk::intToStr(this->namesElemNum) + ")");
      return false;
   }

   return true;
}


