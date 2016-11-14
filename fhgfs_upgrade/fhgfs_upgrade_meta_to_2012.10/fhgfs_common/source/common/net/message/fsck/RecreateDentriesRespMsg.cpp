#include "RecreateDentriesRespMsg.h"

bool RecreateDentriesRespMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;

   {
      // failedCreates
      unsigned failedCreatesBufLen;
      if ( !SerializationFsck::deserializeFsckObjectListPreprocess<FsckFsID>(&buf[bufPos],
         bufLen - bufPos, &failedCreatesStart, &failedCreatesElemNum, &failedCreatesBufLen) )
         return false;

      bufPos += failedCreatesBufLen;
   }

   {
      // createdDentries
      unsigned createdDentriesBufLen;
      if ( !SerializationFsck::deserializeFsckObjectListPreprocess<FsckDirEntry>(&buf[bufPos],
         bufLen - bufPos, &createdDentriesStart, &createdDentriesElemNum, &createdDentriesBufLen) )
         return false;

      bufPos += createdDentriesBufLen;
   }

   {
      // createdInodes
      unsigned createdInodesBufLen;
      if ( !SerializationFsck::deserializeFsckObjectListPreprocess<FsckFileInode>(&buf[bufPos],
         bufLen - bufPos, &createdInodesStart, &createdInodesElemNum, &createdInodesBufLen) )
         return false;

      bufPos += createdInodesBufLen;
   }

   return true;
}

void RecreateDentriesRespMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;

   // failedCreates
   bufPos += SerializationFsck::serializeFsckObjectList(&buf[bufPos], failedCreates);

   // createdDentries
   bufPos += SerializationFsck::serializeFsckObjectList(&buf[bufPos], createdDentries);

   // createdInodes
   bufPos += SerializationFsck::serializeFsckObjectList(&buf[bufPos], createdInodes);
}
