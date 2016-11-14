/*
 * serializePath
 */

#include <Path.h>
#include "Serialization.h"


unsigned Serialization::serializePath(char* buf, Path* path)
{
   size_t bufPos = 0;

   // stringList
   bufPos += serializeStringList(&buf[bufPos], path->getPathElems() );

   // isAbsolute
   bufPos += serializeBool(&buf[bufPos], path->isAbsolute() );

   return bufPos;
}

bool Serialization::deserializePathPreprocess(const char* buf, size_t bufLen,
   struct PathDeserializationInfo* outInfo, unsigned* outLen)
{
   size_t bufPos = 0;

   // stringList
   unsigned elemsBufLen = 0;
   if(unlikely(!deserializeStringListPreprocess(&buf[bufPos], bufLen-bufPos,
      &outInfo->elemNum, &outInfo->elemListStart, &elemsBufLen) ) )
      return false;

   outInfo->elemsBufLen = elemsBufLen;

   bufPos += elemsBufLen;

   // isAbsolute
   unsigned absFieldLen;
   if(!deserializeBool(&buf[bufPos], bufLen-bufPos, &outInfo->isAbsolute, &absFieldLen) )
      return false;

   bufPos += absFieldLen;

   *outLen = bufPos;

   return true;
}

bool Serialization::deserializePath(struct PathDeserializationInfo& info, Path* outPath)
{
   // stringList
   if(unlikely(!deserializeStringList(
      info.elemsBufLen, info.elemNum, info.elemListStart, outPath->getPathElemsNonConst() ) ) )
      return false;

   // isAbsolute
   outPath->setAbsolute(info.isAbsolute);

   return true;
}

unsigned Serialization::serialLenPath(Path* path)
{
   // elemsList + isAbsolute
   return serialLenStringList(path->getPathElems() ) + serialLenBool();
}



