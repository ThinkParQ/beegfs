/*
 * UInt64 (de)serialization methods
 */

#include "Serialization.h"


// =========== list ===========

/**
 * Serialization of an UInt64List.
 */
unsigned Serialization::serializeUInt64List(char* buf, UInt64List* list)
{
   unsigned requiredLen = serialLenUInt64List(list);

   unsigned listSize = list->size();

   size_t bufPos = 0;

   // totalBufLen info field
   bufPos += serializeUInt(&buf[bufPos], requiredLen);

   // elem count info field
   bufPos += serializeUInt(&buf[bufPos], listSize);

   // store each element of the list
   for(UInt64ListConstIter iter=list->begin(); iter != list->end(); iter++)
   {
      bufPos += serializeUInt64(&buf[bufPos], *iter);
   }

   return requiredLen;
}

/**
 * Pre-processes a serialized UIntList64().
 *
 * @return false on error or inconsistency
 */
bool Serialization::deserializeUInt64ListPreprocess(const char* buf, size_t bufLen,
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
      ( (*outLen - bufPos) != (*outElemNum * serialLenUInt64() ) ) ) )
      return false;

   return true;
}

/**
 * Deserializes an UInt64List.
 * (requires pre-processing)
 *
 * @return false on error or inconsistency
 */
bool Serialization::deserializeUInt64List(unsigned listBufLen, unsigned elemNum,
   const char* listStart, UInt64List* outList)
{
   unsigned elemsBufLen =
      listBufLen - serialLenUInt() - serialLenUInt(); // bufLenField & numElemsField

   size_t bufPos = 0;

   for(unsigned i=0; i < elemNum; i++)
   { // read each list element
      uint64_t value = 0; // initilization to prevent wrong compiler warning (unsed variable)
      unsigned valueLen = 0;

      if(unlikely(!deserializeUInt64(&listStart[bufPos], elemsBufLen-bufPos, &value, &valueLen) ) )
         return false;

      bufPos += valueLen;

      outList->push_back(value);
   }

   // TODO: check whether all of the elements were read (=consistency)
   return true;
}

unsigned Serialization::serialLenUInt64List(UInt64List* list)
{
   // bufLen-field + numElems-field + numElems*elemSize
   unsigned requiredLen = serialLenUInt() + serialLenUInt() + list->size()*serialLenUInt64();

   return requiredLen;
}

/**
 * Calculate the size required for serialization. Here we already provide the list size,
 * as std::list->size() needs to iterate over the entire list to compute the size, which is
 * terribly slow, of course.
 */
unsigned Serialization::serialLenUInt64List(size_t size)
{
   // bufLen-field + numElems-field + numElems*elemSize
   unsigned requiredLen = serialLenUInt() + serialLenUInt() + size * serialLenUInt64();

   return requiredLen;
}


// =========== vector ===========

/**
 * Serialize an unsigned integer64 vector
 */
unsigned Serialization::serializeUInt64Vector(char* buf, UInt64Vector* vec)
{
   unsigned requiredLen = serialLenUInt64Vector(vec);
   unsigned listSize = vec->size();

   size_t bufPos = 0;

   // totalBufLen info field
   bufPos += serializeUInt(&buf[bufPos], requiredLen);

   // elem count info field
   bufPos += serializeUInt(&buf[bufPos], listSize);

   for (UInt64VectorConstIter iter=vec->begin(); iter != vec->end(); iter++)
   {
      bufPos += serializeUInt64(&buf[bufPos], *iter);
   }

   return requiredLen;
}

/**
 * Pre-processes a serialized UInt64Vec().
 *
 * @return false on error or inconsistency
 */
bool Serialization::deserializeUInt64VectorPreprocess(const char* buf, size_t bufLen,
   unsigned* outElemNum, const char** outVecStart, unsigned* outLen)
{
   return Serialization::deserializeUInt64ListPreprocess(buf, bufLen, outElemNum, outVecStart,
      outLen);
}

/**
 * Deserializes an UInt64Vec.
 * (requires pre-processing)
 *
 * @return false on error or inconsistency
 */
bool Serialization::deserializeUInt64Vector(unsigned vecBufLen, unsigned elemNum,
   const char* vecStart, UInt64Vector* outVec)
{
   unsigned elemsBufLen =
      vecBufLen - serialLenUInt() - serialLenUInt(); // bufLenField & numElemsField

   size_t bufPos = 0;

   outVec->reserve(elemNum);

   // read each list element
   for(unsigned i=0; i < elemNum; i++)
   {
      uint64_t value;
      unsigned valueLen = 0;

      if(unlikely(!deserializeUInt64(&vecStart[bufPos], elemsBufLen-bufPos, &value, &valueLen) ) )
         return false;

      bufPos += valueLen;

      outVec->push_back(value);
   }

   return true;
}

unsigned Serialization::serialLenUInt64Vector(UInt64Vector* vec)
{
   // bufLen-field + numElems-field + numElems*elemSize
   unsigned requiredLen = serialLenUInt() + serialLenUInt() + vec->size()*serialLenUInt64();

   return requiredLen;
}

unsigned Serialization::serialLenUInt64Vector(size_t size)
{
   // bufLen-field + numElems-field + numElems*elemSize
   unsigned requiredLen = serialLenUInt() + serialLenUInt() + size * serialLenUInt64();

   return requiredLen;
}
