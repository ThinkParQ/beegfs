/*
 * UInt16 (de)serialization methods
 */

#include "Serialization.h"


// =========== list ===========

/**
 * Serialization of an UInt16List.
 */
unsigned Serialization::serializeUInt16List(char* buf, UInt16List* list)
{
   unsigned requiredLen = serialLenUInt16List(list);

   unsigned listSize = list->size();

   size_t bufPos = 0;

   // totalBufLen info field
   bufPos += serializeUInt(&buf[bufPos], requiredLen);

   // elem count info field
   bufPos += serializeUInt(&buf[bufPos], listSize);

   // store each element of the list
   for(UInt16ListConstIter iter=list->begin(); iter != list->end(); iter++)
   {
      bufPos += serializeUShort(&buf[bufPos], *iter);
   }

   return requiredLen;
}

/**
 * Pre-processes a serialized UIntList64().
 *
 * @return false on error or inconsistency
 */
bool Serialization::deserializeUInt16ListPreprocess(const char* buf, size_t bufLen,
   unsigned* outElemNum, const char** outListStart, unsigned* outLen)
{
   size_t bufPos = 0;

   // totalBufLen info field
   unsigned bufLenFieldLen;
   if(unlikely(!deserializeUInt(&buf[bufPos], bufLen-bufPos, outLen, &bufLenFieldLen) ) )
      return false;
   bufPos += bufLenFieldLen;

   // elem count field
   unsigned elemNumFieldLen;
   if(unlikely(!deserializeUInt(&buf[bufPos], bufLen-bufPos, outElemNum, &elemNumFieldLen) ) )
      return false;
   bufPos += elemNumFieldLen;

   *outListStart = &buf[bufPos];

   if(unlikely(
      (*outLen > bufLen) ||
      ( (*outLen - bufPos) != (*outElemNum * serialLenUShort() ) ) ) )
      return false;

   return true;
}

/**
 * Deserializes an UInt16List.
 * (requires pre-processing)
 *
 * @return false on error or inconsistency
 */
bool Serialization::deserializeUInt16List(unsigned listBufLen, unsigned elemNum,
   const char* listStart, UInt16List* outList)
{
   unsigned elemsBufLen =
      listBufLen - serialLenUInt() - serialLenUInt(); // bufLenField & numElemsField

   size_t bufPos = 0;

   for(unsigned i=0; i < elemNum; i++)
   { // read each list element
      uint16_t value = 0; // initilization to prevent wrong compiler warning (unsed variable)
      unsigned valueLen = 0;

      if(unlikely(!deserializeUShort(&listStart[bufPos], elemsBufLen-bufPos, &value, &valueLen) ) )
         return false;

      bufPos += valueLen;

      outList->push_back(value);
   }

   return true;
}

unsigned Serialization::serialLenUInt16List(UInt16List* list)
{
   // bufLen-field + numElems-field + numElems*elemSize
   unsigned requiredLen = serialLenUInt() + serialLenUInt() + list->size()*serialLenUShort();

   return requiredLen;
}

/**
 * Calculate the size required for serialization. Here we already provide the list size,
 * as std::list->size() needs to iterate over the entire list to compute the size, which is
 * terribly slow, of course.
 */
unsigned Serialization::serialLenUInt16List(size_t size)
{
   // bufLen-field + numElems-field + numElems*elemSize
   unsigned requiredLen = serialLenUInt() + serialLenUInt() + size * serialLenUShort();

   return requiredLen;
}


// =========== vector ===========

/**
 * Serialize an unsigned integer64 vector
 */
unsigned Serialization::serializeUInt16Vector(char* buf, UInt16Vector* vec)
{
   unsigned requiredLen = serialLenUInt16Vector(vec);
   unsigned listSize = vec->size();

   size_t bufPos = 0;

   // totalBufLen info field
   bufPos += serializeUInt(&buf[bufPos], requiredLen);

   // elem count info field
   bufPos += serializeUInt(&buf[bufPos], listSize);

   for (UInt16VectorConstIter iter=vec->begin(); iter != vec->end(); iter++)
   {
      bufPos += serializeUShort(&buf[bufPos], *iter);
   }

   return requiredLen;
}

/**
 * Pre-processes a serialized UInt16Vec().
 *
 * @return false on error or inconsistency
 */
bool Serialization::deserializeUInt16VectorPreprocess(const char* buf, size_t bufLen,
   unsigned* outElemNum, const char** outVecStart, unsigned* outLen)
{
   return Serialization::deserializeUInt16ListPreprocess(buf, bufLen, outElemNum, outVecStart,
      outLen);
}

/**
 * Deserializes an UInt16Vec.
 * (requires pre-processing)
 *
 * @return false on error or inconsistency
 */
bool Serialization::deserializeUInt16Vector(unsigned vecBufLen, unsigned elemNum,
   const char* vecStart, UInt16Vector* outVec)
{
   unsigned elemsBufLen =
      vecBufLen - serialLenUInt() - serialLenUInt(); // bufLenField & numElemsField

   size_t bufPos = 0;

   outVec->reserve(elemNum);

   // read each list element
   for(unsigned i=0; i < elemNum; i++)
   {
      uint16_t value;
      unsigned valueLen = 0;

      if(unlikely(!deserializeUShort(&vecStart[bufPos], elemsBufLen-bufPos, &value, &valueLen) ) )
         return false;

      bufPos += valueLen;

      outVec->push_back(value);
   }

   return true;
}

unsigned Serialization::serialLenUInt16Vector(UInt16Vector* vec)
{
   // bufLen-field + numElems-field + numElems*elemSize
   unsigned requiredLen = serialLenUInt() + serialLenUInt() + vec->size()*serialLenUShort();

   return requiredLen;
}

unsigned Serialization::serialLenUInt16Vector(size_t size)
{
   // bufLen-field + numElems-field + numElems*elemSize
   unsigned requiredLen = serialLenUInt() + serialLenUInt() + size * serialLenUShort();

   return requiredLen;
}

