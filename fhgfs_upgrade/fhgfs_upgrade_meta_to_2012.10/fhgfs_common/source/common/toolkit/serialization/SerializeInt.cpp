/*
 * Int (de)serialization methods
 */

#include "Serialization.h"


// =========== list ===========

/**
 * Serialization of a IntList.
 */
unsigned Serialization::serializeIntList(char* buf, IntList* list)
{
   unsigned requiredLen = serialLenIntList(list);

   unsigned listSize = list->size();

   size_t bufPos = 0;

   // totalBufLen info field

   bufPos += serializeUInt(&buf[bufPos], requiredLen);

   // elem count info field

   bufPos += serializeUInt(&buf[bufPos], listSize);


   // store each element of the list

   IntListConstIter iter = list->begin();

   for(unsigned i=0; i < listSize; i++, iter++)
   {
      bufPos += serializeInt(&buf[bufPos], *iter);
   }

   return requiredLen;
}

/**
 * Pre-processes a serialized IntList().
 *
 * @return false on error or inconsistency
 */
bool Serialization::deserializeIntListPreprocess(const char* buf, size_t bufLen,
   unsigned* outElemNum, const char** outListStart, unsigned* outListBufLen)
{
   size_t bufPos = 0;

   // totalBufLen info field
   unsigned bufLenFieldLen;
   if(unlikely(!deserializeUInt(&buf[bufPos], bufLen-bufPos, outListBufLen, &bufLenFieldLen) ) )
      return false;
   bufPos += bufLenFieldLen;

   // elem count field
   unsigned elemNumFieldLen;
   if(unlikely(!deserializeUInt(&buf[bufPos], bufLen-bufPos, outElemNum, &elemNumFieldLen) ) )
      return false;
   bufPos += elemNumFieldLen;

   *outListStart = &buf[bufPos];

   if(unlikely(
      (*outListBufLen > bufLen) ||
      ( (*outListBufLen - bufPos) != (*outElemNum * serialLenInt() ) ) ) )
      return false;

   return true;
}

/**
 * Deserializes a IntList.
 * (requires pre-processing)
 *
 * @return false on error or inconsistency
 */
bool Serialization::deserializeIntList(unsigned listBufLen, unsigned elemNum,
   const char* listStart, IntList* outList)
{
   unsigned elemsBufLen =
      listBufLen - serialLenUInt() - serialLenUInt(); // bufLenField & numElemsField

   size_t bufPos = 0;

   // read each list element

   for(unsigned i=0; i < elemNum; i++)
   {
      int value;
      unsigned valueLen = 0;

      if(unlikely(!deserializeInt(&listStart[bufPos], elemsBufLen-bufPos, &value, &valueLen) ) )
         return false;

      bufPos += valueLen;

      outList->push_back(value);
   }

   // check whether all of the elements were read (=consistency)
   return true;
}

unsigned Serialization::serialLenIntList(IntList* list)
{
   // bufLen-field + numElems-field + numElems*elemSize
   unsigned requiredLen = serialLenUInt() + serialLenUInt() + list->size()*serialLenInt();

   return requiredLen;
}



// =========== vector ===========

/**
 * Serialize an integer vector
 */
unsigned Serialization::serializeIntVector(char* buf, IntVector* vec)
{
   unsigned requiredLen = serialLenIntVector(vec);

   unsigned listSize = vec->size();

   size_t bufPos = 0;

   // totalBufLen info field
   bufPos += serializeUInt(&buf[bufPos], requiredLen);

   // elem count info field
   bufPos += serializeUInt(&buf[bufPos], listSize);

   for (IntVectorConstIter iter=vec->begin(); iter != vec->end(); iter++)
   {
      bufPos += serializeInt(&buf[bufPos], *iter);
   }

   return requiredLen;
}

/**
 * Pre-processes a serialized IntVec().
 *
 * @return false on error or inconsistency
 */
bool Serialization::deserializeIntVectorPreprocess(const char* buf, size_t bufLen,
   unsigned* outElemNum, const char** outVecStart, unsigned* outLen)
{
   return Serialization::deserializeIntListPreprocess(buf, bufLen, outElemNum, outVecStart, outLen);
}

/**
 * Deserializes an IntVec.
 * (requires pre-processing)
 *
 * @return false on error or inconsistency
 */
bool Serialization::deserializeIntVector(unsigned listBufLen, unsigned elemNum,
   const char* listStart, IntVector* outVec)
{
   unsigned elemsBufLen =
      listBufLen - serialLenUInt() - serialLenUInt(); // bufLenField & numElemsField

   size_t bufPos = 0;

   outVec->reserve(elemNum);

   // read each list element
   for(unsigned i=0; i < elemNum; i++)
   {
      int value;
      unsigned valueLen = 0;

      if(unlikely(!deserializeInt(&listStart[bufPos], elemsBufLen-bufPos, &value, &valueLen) ) )
         return false;

      bufPos += valueLen;

      outVec->push_back(value);
   }

   return true;
}

unsigned Serialization::serialLenIntVector(IntVector* vec)
{
   // bufLen-field + numElems-field + numElems*elemSize
   unsigned requiredLen = serialLenUInt() + serialLenUInt() + vec->size()*serialLenInt();

   return requiredLen;
}

