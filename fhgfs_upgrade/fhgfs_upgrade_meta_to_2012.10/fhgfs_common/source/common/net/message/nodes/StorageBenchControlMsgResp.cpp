#include "StorageBenchControlMsgResp.h"

bool StorageBenchControlMsgResp::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;

   // status

   unsigned statusBufLen;

   if(!Serialization::deserializeInt(&buf[bufPos], bufLen-bufPos,
      &this->status, &statusBufLen) )
      return false;

   bufPos += statusBufLen;

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

   // errorCode

   unsigned errorCodeBufLen;

   if(!Serialization::deserializeInt(&buf[bufPos], bufLen-bufPos,
      &this->errorCode, &errorCodeBufLen) )
      return false;

   bufPos += errorCodeBufLen;

   // resultTargetIDs

   if(!Serialization::deserializeUInt16ListPreprocess(&buf[bufPos], bufLen-bufPos,
         &this->resultTargetIDsElemNum, &this->resultTargetIDsListStart,
         &this->resultTargetIDsBufLen) )
      return false;

   bufPos += this->resultTargetIDsBufLen;

   // resultInt64List

   if(!Serialization::deserializeInt64ListPreprocess(&buf[bufPos], bufLen-bufPos,
         &this->resultValuesElemNum, &this->resultValuesListStart, &this->resultValuesBufLen) )
      return false;

   bufPos += this->resultValuesBufLen;


   return true;
}

void StorageBenchControlMsgResp::serializePayload(char* buf)
{
   size_t bufPos = 0;

   // status
   bufPos += Serialization::serializeInt(&buf[bufPos], this->status);

   // action
   bufPos += Serialization::serializeInt(&buf[bufPos], this->action);

   // type
   bufPos += Serialization::serializeInt(&buf[bufPos], this->type);

   // errorCode
   bufPos += Serialization::serializeInt(&buf[bufPos], this->errorCode);

   // resultTargetIDs
   bufPos += Serialization::serializeUInt16List(&buf[bufPos], &this->resultTargetIDs);

   // resultInt64List
   bufPos += Serialization::serializeInt64List(&buf[bufPos], &this->resultValues);
}
