#include "LinkToLostAndFoundMsg.h"

bool LinkToLostAndFoundMsg::deserializePayload(const char* buf, size_t bufLen)
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

   { //lostAndFoundInfo
      unsigned lostAndFoundInfoBufLen = 0;

      if (!lostAndFoundInfo.deserialize(&buf[bufPos], bufLen-bufPos, &lostAndFoundInfoBufLen))
         return false;

      bufPos += lostAndFoundInfoBufLen;
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

   return true;
}

void LinkToLostAndFoundMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;
   
   // entryType
   bufPos += Serialization::serializeInt(&buf[bufPos], (int)entryType);

   //lostAndFoundInfo
   bufPos += lostAndFoundInfoPtr->serialize(&buf[bufPos]);

   if (FsckDirEntryType_ISDIR(this->entryType))
      bufPos += SerializationFsck::serializeFsckDirInodeList(&buf[bufPos], dirInodes);
   else
      bufPos += SerializationFsck::serializeFsckFileInodeList(&buf[bufPos], fileInodes);
}

