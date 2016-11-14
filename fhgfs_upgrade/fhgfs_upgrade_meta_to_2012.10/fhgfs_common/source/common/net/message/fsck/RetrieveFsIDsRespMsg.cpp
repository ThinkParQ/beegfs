#include "RetrieveFsIDsRespMsg.h"

void RetrieveFsIDsRespMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;

   // currentContDirID
   bufPos += Serialization::serializeStrAlign4(&buf[bufPos], currentContDirIDLen,
      currentContDirID);

   // newHashDirOffset
   bufPos += Serialization::serializeInt64(&buf[bufPos], newHashDirOffset);

   // newContDirOffset
   bufPos += Serialization::serializeInt64(&buf[bufPos], newContDirOffset);

   // fsckFsIDs
   bufPos += SerializationFsck::serializeFsckFsIDList(&buf[bufPos], fsckFsIDs);
}

bool RetrieveFsIDsRespMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;

   // currentContDirName
   {
      unsigned currentContDirNameBufLen;
      if ( !Serialization::deserializeStrAlign4(&buf[bufPos], bufLen - bufPos,
         &currentContDirIDLen, &currentContDirID, &currentContDirNameBufLen) )
         return false;

      bufPos += currentContDirNameBufLen;
   }

   {
      // newHashDirOffset
      unsigned newHashDirOffsetBufLen;

      if ( !Serialization::deserializeInt64(&buf[bufPos], bufLen - bufPos, &newHashDirOffset,
         &newHashDirOffsetBufLen) )
         return false;

      bufPos += newHashDirOffsetBufLen;
   }

   {
      // newContDirOffset
      unsigned newContDirOffsetBufLen;

      if ( !Serialization::deserializeInt64(&buf[bufPos], bufLen - bufPos, &newContDirOffset,
         &newContDirOffsetBufLen) )
         return false;

      bufPos += newContDirOffsetBufLen;
   }

   {
      // fsckFsIDs
      if ( !SerializationFsck::deserializeFsckFsIDListPreprocess(&buf[bufPos], bufLen - bufPos,
         &fsckFsIDsStart, &fsckFsIDsElemNum, &fsckFsIDsBufLen) )
         return false;

      bufPos += fsckFsIDsBufLen;
   }
   
   return true;
}

