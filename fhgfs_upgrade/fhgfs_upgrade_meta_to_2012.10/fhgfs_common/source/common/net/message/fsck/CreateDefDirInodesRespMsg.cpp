#include "CreateDefDirInodesRespMsg.h"

bool CreateDefDirInodesRespMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;

   //failedInodeIDs
   if (!Serialization::deserializeStringListPreprocess(&buf[bufPos], bufLen-bufPos,
      &failedInodeIDsElemNum, &failedInodeIDsStart, &failedInodeIDsBufLen))
      return false;

   bufPos += failedInodeIDsBufLen;

   //createdInodes
   if (!SerializationFsck::deserializeFsckDirInodeListPreprocess(&buf[bufPos], bufLen-bufPos,
      &createdInodesStart, &createdInodesElemNum, &createdInodesBufLen))
      return false;

   bufPos += createdInodesBufLen;

   return true;
}

void CreateDefDirInodesRespMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;

   // entryIDs
   bufPos += Serialization::serializeStringList(&buf[bufPos], failedInodeIDs);

   // createdInodes
   bufPos += SerializationFsck::serializeFsckDirInodeList(&buf[bufPos], createdInodes);
}
