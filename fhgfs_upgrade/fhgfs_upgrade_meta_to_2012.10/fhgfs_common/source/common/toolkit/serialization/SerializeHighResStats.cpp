/*
 * Serialization of High Resolution Stats
 */

#include "Serialization.h"


/**
 * Serialization of a HighResStatsList.
 *
 * @return 0 on error, used buffer size otherwise
 */
unsigned Serialization::serializeHighResStatsList(char* buf, HighResStatsList* list)
{
   unsigned listSize = list->size();

   size_t bufPos = 0;

   // elem count info field

   bufPos += serializeUInt(&buf[bufPos], listSize);


   // serialize each element of the list

   HighResStatsListIter iter = list->begin();

   for(unsigned i=0; i < listSize; i++, iter++)
   {
      // statsTimeMS
      bufPos += Serialization::serializeUInt64(&buf[bufPos], iter->rawVals.statsTimeMS);

      // busyWorkers
      bufPos += Serialization::serializeUInt(&buf[bufPos], iter->rawVals.busyWorkers);

      // queuedRequests
      bufPos += Serialization::serializeUInt(&buf[bufPos], iter->rawVals.queuedRequests);

      // diskWriteBytes
      bufPos += Serialization::serializeUInt64(&buf[bufPos], iter->incVals.diskWriteBytes);

      // diskReadBytes
      bufPos += Serialization::serializeUInt64(&buf[bufPos], iter->incVals.diskReadBytes);

      // netSendBytes
      bufPos += Serialization::serializeUInt64(&buf[bufPos], iter->incVals.netSendBytes);

      // netRecvBytes
      bufPos += Serialization::serializeUInt64(&buf[bufPos], iter->incVals.netRecvBytes);

      // workRequests
      bufPos += Serialization::serializeUInt(&buf[bufPos], iter->incVals.workRequests);
   }

   return bufPos;
}

/**
 * Pre-processes a serialized HighResStatsList for deserialization via
 * deserializeHighResStatsList().
 *
 * @return false on error or inconsistency
 */
bool Serialization::deserializeHighResStatsListPreprocess(const char* buf, size_t bufLen,
   unsigned* outListElemNum, const char** outListStart, unsigned* outLen)
{
   unsigned singleElemSize = (5 * serialLenUInt64() ) + (3 * serialLenUInt() );

   size_t bufPos = 0;

   unsigned elemNumFieldLen;
   if(unlikely(!deserializeUInt(&buf[bufPos], bufLen-bufPos, outListElemNum, &elemNumFieldLen) ) )
      return false;
   bufPos += elemNumFieldLen;

   *outLen = bufPos + *outListElemNum * singleElemSize;

   if(unlikely(bufLen < *outLen) )
      return false;

   *outListStart = &buf[bufPos];

   return true;
}

/**
 * Deserializes a HighResStatsList.
 * (requires pre-processing)
 */
void Serialization::deserializeHighResStatsList(unsigned listElemNum, const char* listStart,
   HighResStatsList* outList)
{
   const char* currentListPos = listStart;
   unsigned fieldLen;

   HighResolutionStats currentStats;
   memset(&currentStats, 0, sizeof(currentStats) ); // clear unused fields

   for(unsigned i=0; i < listElemNum; i++)
   {
      deserializeUInt64(currentListPos, ~0, &currentStats.rawVals.statsTimeMS, &fieldLen);
      currentListPos += fieldLen;

      deserializeUInt(currentListPos, ~0, &currentStats.rawVals.busyWorkers, &fieldLen);
      currentListPos += fieldLen;

      deserializeUInt(currentListPos, ~0, &currentStats.rawVals.queuedRequests, &fieldLen);
      currentListPos += fieldLen;

      deserializeUInt64(currentListPos, ~0, &currentStats.incVals.diskWriteBytes, &fieldLen);
      currentListPos += fieldLen;

      deserializeUInt64(currentListPos, ~0, &currentStats.incVals.diskReadBytes, &fieldLen);
      currentListPos += fieldLen;

      deserializeUInt64(currentListPos, ~0, &currentStats.incVals.netSendBytes, &fieldLen);
      currentListPos += fieldLen;

      deserializeUInt64(currentListPos, ~0, &currentStats.incVals.netRecvBytes, &fieldLen);
      currentListPos += fieldLen;

      deserializeUInt(currentListPos, ~0, &currentStats.incVals.workRequests, &fieldLen);
      currentListPos += fieldLen;


      outList->push_back(currentStats);
   }

}

unsigned Serialization::serialLenHighResStatsList(HighResStatsList* list)
{
   // note: singleElemSize is also computed in deserializeHighResStatsListPreprocess()

   unsigned singleElemSize = (5 * serialLenUInt64() ) + (3 * serialLenUInt() );

   // elem count info field + elements
   return serialLenUInt() + (list->size() * singleElemSize);
}


