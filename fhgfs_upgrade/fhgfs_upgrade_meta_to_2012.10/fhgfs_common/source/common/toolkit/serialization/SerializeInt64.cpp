/*
 * Int64 (de)serialization methods
 */

#include "Serialization.h"


// =========== list ===========

/**
 * Serialization of a IntList.
 */
unsigned Serialization::serializeInt64List(char* buf, Int64List* list)
{
   unsigned requiredLen = serialLenInt64List(list);

   unsigned listSize = list->size();

   size_t bufPos = 0;

   // totalBufLen info field
   bufPos += serializeUInt(&buf[bufPos], requiredLen);

   // elem count info field
   bufPos += serializeUInt(&buf[bufPos], listSize);

   // store each element of the list
   for(Int64ListConstIter iter=list->begin(); iter != list->end(); iter++)
   {
      bufPos += serializeInt64(&buf[bufPos], *iter);
   }

   return requiredLen;
}

/**
 * Pre-processes a serialized IntList64().
 *
 * @return false on error or inconsistency
 */
bool Serialization::deserializeInt64ListPreprocess(const char* buf, size_t bufLen,
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
      ( (*outLen - bufPos) != (*outElemNum * serialLenInt64() ) ) ) )
      return false;

   return true;
}

/**
 * Deserializes a Int64List.
 * (requires pre-processing)
 *
 * @return false on error or inconsistency
 */
bool Serialization::deserializeInt64List(unsigned listBufLen, unsigned elemNum,
   const char* listStart, Int64List* outList)
{
   unsigned elemsBufLen =
      listBufLen - serialLenUInt() - serialLenUInt(); // bufLenField & numElemsField

   size_t bufPos = 0;

   for(unsigned i=0; i < elemNum; i++)
   { // read each list element
      int64_t value;
      unsigned valueLen = 0;

      if(unlikely(!deserializeInt64(&listStart[bufPos], elemsBufLen-bufPos, &value, &valueLen) ) )
         return false;

      bufPos += valueLen;

      outList->push_back(value);
   }

   // TODO: check whether all of the elements were read (=consistency)
   return true;
}

unsigned Serialization::serialLenInt64List(Int64List* list)
{
   // bufLen-field + numElems-field + numElems*elemSize
   unsigned requiredLen = serialLenUInt() + serialLenUInt() + list->size()*serialLenInt64();

   return requiredLen;
}

/**
 * Calculate the size required for serialization. Here we already provide the list size,
 * as std::list->size() needs to iterate over the entire list to compute the size, which is
 * terribly slow, of course.
 */
unsigned Serialization::serialLenInt64List(size_t size)
{
   // bufLen-field + numElems-field + numElems*elemSize
   unsigned requiredLen = serialLenUInt() + serialLenUInt() + size * serialLenInt64();

   return requiredLen;
}


// =========== vector ===========

/**
 * Serialize an integer vector
 */
unsigned Serialization::serializeInt64Vector(char* buf, Int64Vector* vec)
{
   unsigned requiredLen = serialLenInt64Vector(vec);
   unsigned listSize = vec->size();

   size_t bufPos = 0;

   // totalBufLen info field
   bufPos += serializeUInt(&buf[bufPos], requiredLen);

   // elem count info field
   bufPos += serializeUInt(&buf[bufPos], listSize);

   for (Int64VectorConstIter iter=vec->begin(); iter != vec->end(); iter++)
   {
      bufPos += serializeInt64(&buf[bufPos], *iter);
   }

   return requiredLen;
}

/**
 * Pre-processes a serialized Int64Vec().
 *
 * @return false on error or inconsistency
 */
bool Serialization::deserializeInt64VectorPreprocess(const char* buf, size_t bufLen,
   unsigned* outElemNum, const char** outVecStart, unsigned* outLen)
{
   return Serialization::deserializeInt64ListPreprocess(buf, bufLen, outElemNum, outVecStart, outLen);
}

/**
 * Deserializes an Int64Vec.
 * (requires pre-processing)
 *
 * @return false on error or inconsistency
 */
bool Serialization::deserializeInt64Vector(unsigned listBufLen, unsigned elemNum,
   const char* listStart, Int64Vector* outVec)
{
   unsigned elemsBufLen =
      listBufLen - serialLenUInt() - serialLenUInt(); // bufLenField & numElemsField

   size_t bufPos = 0;

   outVec->reserve(elemNum);

   // read each list element
   for(unsigned i=0; i < elemNum; i++)
   {
      int64_t value;
      unsigned valueLen = 0;

      if(unlikely(!deserializeInt64(&listStart[bufPos], elemsBufLen-bufPos, &value, &valueLen) ) )
         return false;

      bufPos += valueLen;

      outVec->push_back(value);
   }

   return true;
}

unsigned Serialization::serialLenInt64Vector(Int64Vector* vec)
{
   // bufLen-field + numElems-field + numElems*elemSize
   unsigned requiredLen = serialLenUInt() + serialLenUInt() + vec->size()*serialLenInt64();

   return requiredLen;
}

