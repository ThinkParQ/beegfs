#include "RetrieveDirEntriesRespMsg.h"

void RetrieveDirEntriesRespMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;

   // currentContDirID
   bufPos += Serialization::serializeStrAlign4(&buf[bufPos], currentContDirIDLen,
      currentContDirID);

   // newHashDirOffset
   bufPos += Serialization::serializeInt64(&buf[bufPos], newHashDirOffset);

   // newContDirOffset
   bufPos += Serialization::serializeInt64(&buf[bufPos], newContDirOffset);

   // contDirs
   bufPos += SerializationFsck::serializeFsckContDirList(&buf[bufPos], fsckContDirs);

   // dirEntries
   bufPos += SerializationFsck::serializeFsckDirEntryList(&buf[bufPos], fsckDirEntries);
   
   // inlinedFileInodes
   bufPos += SerializationFsck::serializeFsckFileInodeList(&buf[bufPos], inlinedFileInodes);
}

bool RetrieveDirEntriesRespMsg::deserializePayload(const char* buf, size_t bufLen)
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
      // fsckContDirs
      if ( !SerializationFsck::deserializeFsckContDirListPreprocess(&buf[bufPos], bufLen - bufPos,
         &fsckContDirsStart, &fsckContDirsElemNum, &fsckContDirsBufLen) )
         return false;

      bufPos += fsckContDirsBufLen;
   }

   {
      // fsckDirEntries
      if ( !SerializationFsck::deserializeFsckDirEntryListPreprocess(&buf[bufPos], bufLen - bufPos,
         &fsckDirEntriesStart, &fsckDirEntriesElemNum, &fsckDirEntriesBufLen) )
         return false;

      bufPos += fsckDirEntriesBufLen;
   }

   {
      // inlinedFileInodes
      if ( !SerializationFsck::deserializeFsckFileInodeListPreprocess(&buf[bufPos], bufLen - bufPos,
         &inlinedFileInodesStart, &inlinedFileInodesElemNum, &inlinedFileInodesBufLen) )
         return false;

      bufPos += inlinedFileInodesBufLen;
   }
   
   return true;
}

