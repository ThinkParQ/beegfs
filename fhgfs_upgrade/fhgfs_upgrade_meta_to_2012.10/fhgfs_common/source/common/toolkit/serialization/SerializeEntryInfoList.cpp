#include <common/storage/EntryInfo.h>
#include "Serialization.h"

// =========== serializeEntryInfoList ===========

/**
 * Serialization of a EntryInfoList.
 *
 * @return 0 on error, used buffer size otherwise
 */
unsigned Serialization::serializeEntryInfoList(char* buf, EntryInfoList* entryInfoList)
{
   unsigned entryInfoListSize = entryInfoList->size();

   size_t bufPos = 0;

   // elem count info field

   bufPos += serializeUInt(&buf[bufPos], entryInfoListSize);

   // serialize each element of the entryInfoList

   EntryInfoListIter iter = entryInfoList->begin();

   for ( unsigned i = 0; i < entryInfoListSize; i++, iter++ )
   {
      EntryInfo entryInfo = *iter;

      bufPos += entryInfo.serialize(&buf[bufPos]);
   }

   return bufPos;
}

unsigned Serialization::serialLenEntryInfoList(EntryInfoList* entryInfoList)
{
   unsigned entryInfoListSize = entryInfoList->size();

   size_t bufPos = 0;

   bufPos += serialLenUInt();

   EntryInfoListIter iter = entryInfoList->begin();

   for ( unsigned i = 0; i < entryInfoListSize; i++, iter++ )
   {
      EntryInfo entryInfo = *iter;

      bufPos += entryInfo.serialLen();
   }

   return bufPos;
}

/**
 * Pre-processes a serialized EntryInfoList for deserialization via deserializeEntryInfoList().
 *
 * @return false on error or inconsistency
 */
bool Serialization::deserializeEntryInfoListPreprocess(const char* buf, size_t bufLen,
   unsigned *outElemNum, const char** outInfoStart, unsigned* outLen)
{
   // Note: This is a fairly inefficient implementation (requiring redundant pre-processing),
   //    but efficiency doesn't matter for the typical use-cases of this method

   size_t bufPos = 0;

   // elem count info field
   unsigned elemNumFieldLen;

   if ( unlikely(!deserializeUInt(&buf[bufPos], bufLen-bufPos, outElemNum,
         &elemNumFieldLen) ) )
      return false;

   bufPos += elemNumFieldLen;

   *outInfoStart = &buf[bufPos];

   for ( unsigned i = 0; i < *outElemNum; i++ )
   {
      EntryInfo entryInfo;
      unsigned entryInfoLen;

      if (unlikely(!entryInfo.deserialize(&buf[bufPos], bufLen-bufPos, &entryInfoLen)))
         return false;

      bufPos += entryInfoLen;
   }

   *outLen = bufPos;

   return true;
}

void Serialization::deserializeEntryInfoList(unsigned entryInfoListLen,
   unsigned entryInfoListElemNum, const char* entryInfoListStart, EntryInfoList *outEntryInfoList)
{
   const char* buf = entryInfoListStart;
   size_t bufPos = 0;
   size_t bufLen = ~0;

   for(unsigned i=0; i < entryInfoListElemNum; i++)
   {
      EntryInfo entryInfo;
      unsigned entryInfoLen;

      entryInfo.deserialize(&buf[bufPos], bufLen-bufPos, &entryInfoLen);

      bufPos += entryInfoLen;

      outEntryInfoList->push_back(entryInfo);
   }
}

