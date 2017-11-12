/*
 * AdmonSerialization.cpp
 */


#include "Serialization.h"
#include "common/toolkit/AdmonTk.h"


unsigned Serialization::serializeStorageTargetInfoList(char* buf,
   StorageTargetInfoList *storageTargetInfoList)
{
   unsigned storageTargetInfoListSize = storageTargetInfoList->size();

   size_t bufPos = 0;

   // elem count info field
   bufPos += serializeUInt(&buf[bufPos], storageTargetInfoListSize);

   // serialize each element of the list
   StorageTargetInfoListIter iter = storageTargetInfoList->begin();

   for (unsigned i = 0; i < storageTargetInfoListSize; i++, iter++)
   {
      // targetID
      uint16_t targetID = (*iter).targetID;
      bufPos += serializeUShort(&buf[bufPos], targetID);

      // pathStr
      std::string pathStr = (*iter).pathStr;
      bufPos += serializeStr(&buf[bufPos], pathStr.length(), pathStr.c_str());

      // diskSpaceTotal
      int64_t diskSpaceTotal = (*iter).diskSpaceTotal;
      bufPos += serializeInt64(&buf[bufPos], diskSpaceTotal);

      // diskSpaceFree
      int64_t diskSpaceFree = (*iter).diskSpaceFree;
      bufPos += serializeInt64(&buf[bufPos], diskSpaceFree);
   }

   return bufPos;
}

unsigned Serialization::serialLenStorageTargetInfoList(StorageTargetInfoList* storageTargetInfoList)
{
   unsigned storageTargetInfoListSize = storageTargetInfoList->size();

   size_t bufPos = 0;

   bufPos += serialLenUInt();

   // serialize each element of the list
   StorageTargetInfoListIter iter = storageTargetInfoList->begin();

   for (unsigned i = 0; i < storageTargetInfoListSize; i++, iter++)
   {
      // targetID
      bufPos += serialLenUShort();

      // pathStr
      std::string pathStr = (*iter).pathStr;
      bufPos += serialLenStr(pathStr.length());

      // diskSpaceTotal
      bufPos += serialLenInt64();

      // diskSpaceFree
      bufPos += serialLenInt64();
   }

   return bufPos;
}

bool Serialization::deserializeStorageTargetInfoListPreprocess(const char* buf, size_t bufLen,
   const char** outInfoStart, unsigned *outElemNum, unsigned* outLen)
{
   size_t bufPos = 0;

   unsigned elemNumFieldLen;

   if (unlikely(!deserializeUInt(&buf[bufPos], bufLen-bufPos, outElemNum,
         &elemNumFieldLen) ))
      return false;

   bufPos += elemNumFieldLen;

   *outInfoStart = &buf[bufPos];

   for (unsigned i = 0; i < *outElemNum; i++)
   {
      // targetID
      uint16_t targetID;
      unsigned idBufLen = 0;

      if (unlikely(!Serialization::deserializeUShort(&buf[bufPos], bufLen-bufPos, &targetID,
            &idBufLen) ))
         return false;

      bufPos += idBufLen;

      // pathStr
      unsigned pathStrLen = 0;
      const char* pathStr = NULL;
      unsigned pathStrBufLen = 0;

      if (unlikely(!Serialization::deserializeStr(&buf[bufPos], bufLen-bufPos,
            &pathStrLen, &pathStr, &pathStrBufLen) ))
         return false;

      bufPos += pathStrBufLen;

      // diskSpaceTotal
      int64_t diskSpaceTotal = 0;
      unsigned diskSpaceTotalBufLen = 0;

      if (unlikely(!Serialization::deserializeInt64(&buf[bufPos], bufLen-bufPos, &diskSpaceTotal,
         &diskSpaceTotalBufLen)))
         return false;

      bufPos += diskSpaceTotalBufLen;

      // diskSpaceFree
      int64_t diskSpaceFree = 0;
      unsigned diskSpaceFreeBufLen = 0;

      if (unlikely(!Serialization::deserializeInt64(&buf[bufPos], bufLen-bufPos, &diskSpaceFree,
         &diskSpaceFreeBufLen)))
         return false;

      bufPos += diskSpaceFreeBufLen;
   }

   *outLen = bufPos;

   return true;
}

bool Serialization::deserializeStorageTargetInfoList(unsigned storageTargetInfoListElemNum,
   const char* storageTargetInfoListStart, StorageTargetInfoList* outList)
{
   size_t bufPos = 0;
   size_t bufLen = ~0; // fake bufLen to max value (has already been verified during pre-processing)
   const char* buf = storageTargetInfoListStart;

   for (unsigned i = 0; i < storageTargetInfoListElemNum; i++)
   {
      StorageTargetInfo storageTarget;

      // targetID
      uint16_t targetID = 0;
      unsigned idBufLen = 0;

      Serialization::deserializeUShort(&buf[bufPos], bufLen - bufPos, &targetID, &idBufLen);

      bufPos += idBufLen;

      // pathStr
      unsigned pathStrLen = 0;
      const char* pathStr = NULL;
      unsigned pathStrBufLen = 0;

      Serialization::deserializeStr(&buf[bufPos], bufLen-bufPos, &pathStrLen, &pathStr,
         &pathStrBufLen);

      bufPos += pathStrBufLen;

      // diskSpaceTotal
      int64_t diskSpaceTotal = 0;
      unsigned diskSpaceTotalBufLen = 0;

      Serialization::deserializeInt64(&buf[bufPos], bufLen - bufPos, &diskSpaceTotal,
         &diskSpaceTotalBufLen);

      bufPos += diskSpaceTotalBufLen;

      // diskSpaceFree
      int64_t diskSpaceFree = 0;
      unsigned diskSpaceFreeBufLen = 0;

      Serialization::deserializeInt64(&buf[bufPos], bufLen - bufPos, &diskSpaceFree,
         &diskSpaceFreeBufLen);

      bufPos += diskSpaceFreeBufLen;

      storageTarget.targetID = targetID;
      storageTarget.pathStr = pathStr;
      storageTarget.diskSpaceFree = diskSpaceFree;
      storageTarget.diskSpaceTotal = diskSpaceTotal;
      outList->push_back(storageTarget);
   }

   return true;
}
