#include "StorageBenchControlMsg.h"

bool StorageBenchControlMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;

   // action

   unsigned actionBufLen;

   if(!Serialization::deserializeInt(&buf[bufPos], bufLen-bufPos,
      &this->action, &actionBufLen) )
      return false;

   bufPos += actionBufLen;

   // type

   unsigned typeBufLen;

   if(!Serialization::deserializeInt(&buf[bufPos], bufLen-bufPos,
      &this->type, &typeBufLen) )
      return false;

   bufPos += typeBufLen;

   // blocksize

   unsigned blocksizeBufLen;

   if(!Serialization::deserializeInt64(&buf[bufPos], bufLen-bufPos,
      &this->blocksize, &blocksizeBufLen) )
      return false;

   bufPos += blocksizeBufLen;

   // size

   unsigned sizeBufLen;

   if(!Serialization::deserializeInt64(&buf[bufPos], bufLen-bufPos,
      &this->size, &sizeBufLen) )
      return false;

   bufPos += sizeBufLen;

   // threads

   unsigned threadsBufLen;

   if(!Serialization::deserializeInt(&buf[bufPos], bufLen-bufPos,
      &this->threads, &threadsBufLen) )
      return false;

   bufPos += threadsBufLen;

   // targetIDs

   if(!Serialization::deserializeUInt16ListPreprocess(&buf[bufPos], bufLen-bufPos,
         &this->targetIDsElemNum, &this->targetIDListStart, &this->targetIDsBufLen) )
      return false;

   bufPos += this->targetIDsBufLen;

   return true;
}

void StorageBenchControlMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;

   // action
   bufPos += Serialization::serializeInt(&buf[bufPos], this->action);

   // type
   bufPos += Serialization::serializeInt(&buf[bufPos], this->type);

   // blocksize
   bufPos += Serialization::serializeInt64(&buf[bufPos], this->blocksize);

   // size
   bufPos += Serialization::serializeInt64(&buf[bufPos], this->size);

   // threads
   bufPos += Serialization::serializeInt(&buf[bufPos], this->threads);

   // targetIDs
   bufPos += Serialization::serializeUInt16List(&buf[bufPos], this->targetIDs);
}
