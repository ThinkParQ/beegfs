#include "LinkToLostAndFoundRespMsg.h"

bool LinkToLostAndFoundRespMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;
   
   { // entryType
      unsigned entryTypeBufLen = 0;
      int intEntryType;

      if (!Serialization::deserializeInt(&buf[bufPos], bufLen-bufPos, &intEntryType,
         &entryTypeBufLen))
         return false;

      this->entryType = (FsckDirEntryType)intEntryType;

      bufPos += entryTypeBufLen;
   }

   {
      if (FsckDirEntryType_ISDIR(this->entryType))
      {
         // dirInodes
         if (!SerializationFsck::deserializeFsckDirInodeListPreprocess(&buf[bufPos], bufLen-bufPos,
            &inodesStart, &inodesElemNum, &inodesBufLen))
            return false;
      }
      else
      {
         // fileInodes
         if (!SerializationFsck::deserializeFsckFileInodeListPreprocess(&buf[bufPos], bufLen-bufPos,
            &inodesStart, &inodesElemNum, &inodesBufLen))
            return false;
      }

      bufPos += inodesBufLen;
   }

   { // createdDentries
      if (!SerializationFsck::deserializeFsckDirEntryListPreprocess(&buf[bufPos], bufLen-bufPos,
         &createdDentriesStart, &createdDentriesElemNum, &createdDentriesBufLen))
         return false;

      bufPos += createdDentriesBufLen;
   }


   return true;
}

void LinkToLostAndFoundRespMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;
   
   // entryType
   bufPos += Serialization::serializeInt(&buf[bufPos], (int)entryType);

   if (FsckDirEntryType_ISDIR(this->entryType)) // dirInodes
      bufPos += SerializationFsck::serializeFsckDirInodeList(&buf[bufPos], failedDirInodes);
   else // fileInodes
      bufPos += SerializationFsck::serializeFsckFileInodeList(&buf[bufPos], failedFileInodes);

   // createdDentries
   bufPos += SerializationFsck::serializeFsckDirEntryList(&buf[bufPos], createdDentries);
}

