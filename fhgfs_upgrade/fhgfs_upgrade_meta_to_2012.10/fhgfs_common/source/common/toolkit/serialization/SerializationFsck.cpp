#include "SerializationFsck.h"

SerializationFsck::SerializationFsck()
{
}

SerializationFsck::~SerializationFsck()
{
}

// =========== serializeFsckDirEntryList ===========
/**
 * Serialization of a FsckDirEntryList.
 *
 * @return 0 on error, used buffer size otherwise
 */
unsigned SerializationFsck::serializeFsckDirEntryList(char* buf, FsckDirEntryList *fsckDirEntryList)
{
   return serializeFsckObjectList(buf, fsckDirEntryList);
}

/**
 * Calculate the needed buffer length for serialization of a FsckDirEntryList
 *
 * @return needed buffer size
 */
unsigned SerializationFsck::serialLenFsckDirEntryList(FsckDirEntryList* fsckDirEntryList)
{
   return serialLenFsckObjectList(fsckDirEntryList);
}

/**
 * Pre-processes a serialized FsckDirEntryList for deserialization via
 * deserializeFsckDirEntryList().
 *
 * @return false on error or inconsistency
 */
bool SerializationFsck::deserializeFsckDirEntryListPreprocess(const char* buf, size_t bufLen,
   const char** outInfoStart, unsigned *outElemNum, unsigned* outLen)
{
   return deserializeFsckObjectListPreprocess<FsckDirEntry>(buf, bufLen, outInfoStart, outElemNum,
      outLen);
}

/*
 * deserialize a FsckDirEntryList
 */
void SerializationFsck::deserializeFsckDirEntryList(unsigned listElemNum, const char* listStart,
   FsckDirEntryList* outList)
{
   deserializeFsckObjectList(listElemNum, listStart, outList);
}

// =========== serializeFsckFileInodeList ===========
/**
 * Serialization of a FsckFileInodeList.
 *
 * @return 0 on error, used buffer size otherwise
 */
unsigned SerializationFsck::serializeFsckFileInodeList(char* buf,
   FsckFileInodeList *fsckFileInodeList)
{
   return serializeFsckObjectList(buf, fsckFileInodeList);
}

/**
 * Calculate the needed buffer length for serialization of a FsckFileInodeList
 *
 * @return needed buffer size
 */
unsigned SerializationFsck::serialLenFsckFileInodeList(FsckFileInodeList* fsckFileInodeList)
{
   return serialLenFsckObjectList(fsckFileInodeList);
}

/**
 * Pre-processes a serialized FsckFileInodeList for deserialization via
 * deserializeFsckFileInodeList().
 *
 * @return false on error or inconsistency
 */
bool SerializationFsck::deserializeFsckFileInodeListPreprocess(const char* buf, size_t bufLen,
   const char** outInfoStart, unsigned *outElemNum, unsigned* outLen)
{
   return deserializeFsckObjectListPreprocess<FsckFileInode>(buf, bufLen, outInfoStart, outElemNum,
      outLen);
}

/*
 * deserialize a FsckFileInodeList
 */
void SerializationFsck::deserializeFsckFileInodeList(unsigned listElemNum, const char* listStart,
   FsckFileInodeList* outList)
{
   deserializeFsckObjectList(listElemNum, listStart, outList);
}

// =========== serializeFsckDirInodeList ===========
/**
 * Serialization of a FsckDirInodeList.
 *
 * @return 0 on error, used buffer size otherwise
 */
unsigned SerializationFsck::serializeFsckDirInodeList(char* buf, FsckDirInodeList *fsckDirInodeList)
{
   return serializeFsckObjectList(buf, fsckDirInodeList);
}

/**
 * Calculate the needed buffer length for serialization of a FsckDirInodeList
 *
 * @return needed buffer size
 */
unsigned SerializationFsck::serialLenFsckDirInodeList(FsckDirInodeList* fsckDirInodeList)
{
   return serialLenFsckObjectList(fsckDirInodeList);
}

/**
 * Pre-processes a serialized FsckDirInodeList for deserialization via
 * deserializeFsckDirInodeList().
 *
 * @return false on error or inconsistency
 */
bool SerializationFsck::deserializeFsckDirInodeListPreprocess(const char* buf, size_t bufLen,
   const char** outInfoStart, unsigned *outElemNum, unsigned* outLen)
{
   return deserializeFsckObjectListPreprocess<FsckDirInode>(buf, bufLen, outInfoStart, outElemNum,
      outLen);
}

/*
 * deserialize a FsckDirInodeList
 */
void SerializationFsck::deserializeFsckDirInodeList(unsigned listElemNum, const char* listStart,
   FsckDirInodeList* outList)
{
   deserializeFsckObjectList(listElemNum, listStart, outList);
}

// =========== serializeFsckChunkList ===========
/**
 * Serialization of a FsckChunkList.
 *
 * @return 0 on error, used buffer size otherwise
 */
unsigned SerializationFsck::serializeFsckChunkList(char* buf, FsckChunkList *fsckChunkInodeList)
{
   return serializeFsckObjectList(buf, fsckChunkInodeList);
}

/**
 * Calculate the needed buffer length for serialization of a FsckChunkList
 *
 * @return needed buffer size
 */
unsigned SerializationFsck::serialLenFsckChunkList(FsckChunkList* fsckChunkInodeList)
{
   return serialLenFsckObjectList(fsckChunkInodeList);
}

/**
 * Pre-processes a serialized FsckChunkList for deserialization via deserializeFsckChunkList().
 *
 * @return false on error or inconsistency
 */
bool SerializationFsck::deserializeFsckChunkListPreprocess(const char* buf, size_t bufLen,
   const char** outInfoStart, unsigned *outElemNum, unsigned* outLen)
{
   return deserializeFsckObjectListPreprocess<FsckChunk>(buf, bufLen, outInfoStart, outElemNum,
      outLen);
}

/*
 * deserialize a FsckChunkList
 */
void SerializationFsck::deserializeFsckChunkList(unsigned listElemNum, const char* listStart,
   FsckChunkList* outList)
{
   deserializeFsckObjectList(listElemNum, listStart, outList);
}

// =========== serializeFsckContDirList ===========
/**
 * Serialization of a FsckContDirList.
 *
 * @return 0 on error, used buffer size otherwise
 */
unsigned SerializationFsck::serializeFsckContDirList(char* buf, FsckContDirList *fsckContDirList)
{
   return serializeFsckObjectList(buf, fsckContDirList);
}

/**
 * Calculate the needed buffer length for serialization of a FsckContDirList
 *
 * @return needed buffer size
 */
unsigned SerializationFsck::serialLenFsckContDirList(FsckContDirList* fsckContDirList)
{
   return serialLenFsckObjectList(fsckContDirList);
}

/**
 * Pre-processes a serialized FsckContDirList for deserialization via deserializeFsckContDirList().
 *
 * @return false on error or inconsistency
 */
bool SerializationFsck::deserializeFsckContDirListPreprocess(const char* buf, size_t bufLen,
   const char** outInfoStart, unsigned *outElemNum, unsigned* outLen)
{
   return deserializeFsckObjectListPreprocess<FsckContDir>(buf, bufLen, outInfoStart, outElemNum,
      outLen);
}

/*
 * deserialize a FsckContDirList
 */
void SerializationFsck::deserializeFsckContDirList(unsigned listElemNum, const char* listStart,
   FsckContDirList* outList)
{
   deserializeFsckObjectList(listElemNum, listStart, outList);
}

// =========== serializeFsckFsIDList ===========
/**
 * Serialization of a FsckFsIDList.
 *
 * @return 0 on error, used buffer size otherwise
 */
unsigned SerializationFsck::serializeFsckFsIDList(char* buf, FsckFsIDList *fsckFsIDList)
{
   return serializeFsckObjectList(buf, fsckFsIDList);
}

/**
 * Calculate the needed buffer length for serialization of a FsckFsIDList
 *
 * @return needed buffer size
 */
unsigned SerializationFsck::serialLenFsckFsIDList(FsckFsIDList* fsckFsIDList)
{
   return serialLenFsckObjectList(fsckFsIDList);
}

/**
 * Pre-processes a serialized FsckDirEntryList for deserialization via
 * deserializeFsckFsIDList().
 *
 * @return false on error or inconsistency
 */
bool SerializationFsck::deserializeFsckFsIDListPreprocess(const char* buf, size_t bufLen,
   const char** outInfoStart, unsigned *outElemNum, unsigned* outLen)
{
   return deserializeFsckObjectListPreprocess<FsckFsID>(buf, bufLen, outInfoStart, outElemNum,
      outLen);
}

/*
 * deserialize a FsckFsIDList
 */
void SerializationFsck::deserializeFsckFsIDList(unsigned listElemNum, const char* listStart,
   FsckFsIDList* outList)
{
   deserializeFsckObjectList(listElemNum, listStart, outList);
}

/*// =========== serializeFsckDirEntryList ===========
*
 * Serialization of a FsckDirEntryList.
 *
 * @return 0 on error, used buffer size otherwise

unsigned SerializationFsck::serializeFsckDirEntryList(char* buf, FsckDirEntryList *fsckDirEntryList)
{
   unsigned listSize = fsckDirEntryList->size();

   size_t bufPos = 0;

   // elem count info field

   bufPos += serializeUInt(&buf[bufPos], listSize);

   // serialize each element of the list

   FsckDirEntryListIter iter = fsckDirEntryList->begin();

   for ( unsigned i = 0; i < listSize; i++, iter++ )
   {
      FsckDirEntry element = *iter;

      bufPos += element.serialize(&buf[bufPos]);
   }

   return bufPos;
}

*
 * Calculate the needed buffer length for serialization of a FsckDirEntryList
 *
 * @return needed buffer size

unsigned SerializationFsck::serialLenFsckDirEntryList(FsckDirEntryList* fsckDirEntryList)
{
   unsigned listSize = fsckDirEntryList->size();

   size_t bufPos = 0;

   bufPos += serialLenUInt();

   FsckDirEntryListIter iter = fsckDirEntryList->begin();

   for ( unsigned i = 0; i < listSize; i++, iter++ )
   {
      FsckDirEntry element = *iter;

      bufPos += element.serialLen();
   }

   return bufPos;
}

*
 * Pre-processes a serialized FsckDirEntryList for deserialization via
 * deserializeFsckDirEntryList().
 *
 * @return false on error or inconsistency

bool SerializationFsck::deserializeFsckDirEntryListPreprocess(const char* buf, size_t bufLen,
   const char** outInfoStart, unsigned *outElemNum, unsigned* outLen)
{
   // Note: This is a fairly inefficient implementation (requiring redundant pre-processing),
   //    but efficiency doesn't matter for the typical use-cases of this method

   size_t bufPos = 0;

   // elem count info field
   unsigned elemNumFieldLen;

   if ( unlikely(!deserializeUInt(&buf[bufPos], bufLen-bufPos, outElemNum,
         &elemNumFieldLen) ) )
      return false;

   bufPos += elemNumFieldLen;

   *outInfoStart = &buf[bufPos];

   for ( unsigned i = 0; i < *outElemNum; i++ )
   {
      FsckDirEntry element;
      unsigned elementLen;

      if (unlikely(!element.deserialize(&buf[bufPos], bufLen-bufPos, &elementLen)))
         return false;

      bufPos += elementLen;
   }

   *outLen = bufPos;

   return true;
}


 * deserialize a FsckDirEntryList

void SerializationFsck::deserializeFsckDirEntryList(unsigned listElemNum, const char* listStart,
   FsckDirEntryList* outList)
{
   const char* buf = listStart;
   size_t bufPos = 0;
   size_t bufLen = ~0;

   for(unsigned i=0; i < listElemNum; i++)
   {
      FsckDirEntry element;
      unsigned elementLen;

      element.deserialize(&buf[bufPos], bufLen-bufPos, &elementLen);

      bufPos += elementLen;

      outList->push_back(element);
   }
}

// =========== serializeFsckFileInodeList ===========
*
 * Serialization of a FsckFileInodeList.
 *
 * @return 0 on error, used buffer size otherwise

unsigned SerializationFsck::serializeFsckFileInodeList(char* buf, FsckFileInodeList *fsckFileInodeList)
{
   unsigned listSize = fsckFileInodeList->size();

   size_t bufPos = 0;

   // elem count info field

   bufPos += serializeUInt(&buf[bufPos], listSize);

   // serialize each element of the list

   FsckFileInodeListIter iter = fsckFileInodeList->begin();

   for ( unsigned i = 0; i < listSize; i++, iter++ )
   {
      FsckFileInode element = *iter;

      bufPos += element.serialize(&buf[bufPos]);
   }

   return bufPos;
}

*
 * Calculate the needed buffer length for serialization of a FsckFileInodeList
 *
 * @return needed buffer size

unsigned SerializationFsck::serialLenFsckFileInodeList(FsckFileInodeList* fsckFileInodeList)
{
   unsigned listSize = fsckFileInodeList->size();

   size_t bufPos = 0;

   bufPos += serialLenUInt();

   FsckFileInodeListIter iter = fsckFileInodeList->begin();

   for ( unsigned i = 0; i < listSize; i++, iter++ )
   {
      FsckFileInode element = *iter;

      bufPos += element.serialLen();
   }

   return bufPos;
}

*
 * Pre-processes a serialized FsckFileInodeList for deserialization via
 * deserializeFsckFileInodeList().
 *
 * @return false on error or inconsistency

bool SerializationFsck::deserializeFsckFileInodeListPreprocess(const char* buf, size_t bufLen,
   const char** outInfoStart, unsigned *outElemNum, unsigned* outLen)
{
   // Note: This is a fairly inefficient implementation (requiring redundant pre-processing),
   //    but efficiency doesn't matter for the typical use-cases of this method

   size_t bufPos = 0;

   // elem count info field
   unsigned elemNumFieldLen;

   if ( unlikely(!deserializeUInt(&buf[bufPos], bufLen-bufPos, outElemNum,
         &elemNumFieldLen) ) )
      return false;

   bufPos += elemNumFieldLen;

   *outInfoStart = &buf[bufPos];

   for ( unsigned i = 0; i < *outElemNum; i++ )
   {
      FsckFileInode element;
      unsigned elementLen;

      if (unlikely(!element.deserialize(&buf[bufPos], bufLen-bufPos, &elementLen)))
         return false;

      bufPos += elementLen;
   }

   *outLen = bufPos;

   return true;
}


 * deserialize a FsckFileInodeList

void SerializationFsck::deserializeFsckFileInodeList(unsigned listElemNum, const char* listStart,
   FsckFileInodeList* outList)
{
   const char* buf = listStart;
   size_t bufPos = 0;
   size_t bufLen = ~0;

   for(unsigned i=0; i < listElemNum; i++)
   {
      FsckFileInode element;
      unsigned elementLen;

      element.deserialize(&buf[bufPos], bufLen-bufPos, &elementLen);

      bufPos += elementLen;

      outList->push_back(element);
   }
}

// =========== serializeFsckDirInodeList ===========
*
 * Serialization of a FsckDirInodeList.
 *
 * @return 0 on error, used buffer size otherwise

unsigned SerializationFsck::serializeFsckDirInodeList(char* buf, FsckDirInodeList *fsckDirInodeList)
{
   unsigned listSize = fsckDirInodeList->size();

   size_t bufPos = 0;

   // elem count info field

   bufPos += serializeUInt(&buf[bufPos], listSize);

   // serialize each element of the list

   FsckDirInodeListIter iter = fsckDirInodeList->begin();

   for ( unsigned i = 0; i < listSize; i++, iter++ )
   {
      FsckDirInode element = *iter;

      bufPos += element.serialize(&buf[bufPos]);
   }

   return bufPos;
}

*
 * Calculate the needed buffer length for serialization of a FsckDirInodeList
 *
 * @return needed buffer size

unsigned SerializationFsck::serialLenFsckDirInodeList(FsckDirInodeList* fsckDirInodeList)
{
   unsigned listSize = fsckDirInodeList->size();

   size_t bufPos = 0;

   bufPos += serialLenUInt();

   FsckDirInodeListIter iter = fsckDirInodeList->begin();

   for ( unsigned i = 0; i < listSize; i++, iter++ )
   {
      FsckDirInode element = *iter;

      bufPos += element.serialLen();
   }

   return bufPos;
}

*
 * Pre-processes a serialized FsckDirInodeList for deserialization via
 * deserializeFsckDirInodeList().
 *
 * @return false on error or inconsistency

bool SerializationFsck::deserializeFsckDirInodeListPreprocess(const char* buf, size_t bufLen,
   const char** outInfoStart, unsigned *outElemNum, unsigned* outLen)
{
   // Note: This is a fairly inefficient implementation (requiring redundant pre-processing),
   //    but efficiency doesn't matter for the typical use-cases of this method

   size_t bufPos = 0;

   // elem count info field
   unsigned elemNumFieldLen;

   if ( unlikely(!deserializeUInt(&buf[bufPos], bufLen-bufPos, outElemNum,
         &elemNumFieldLen) ) )
      return false;

   bufPos += elemNumFieldLen;

   *outInfoStart = &buf[bufPos];

   for ( unsigned i = 0; i < *outElemNum; i++ )
   {
      FsckDirInode element;
      unsigned elementLen;

      if (unlikely(!element.deserialize(&buf[bufPos], bufLen-bufPos, &elementLen)))
         return false;

      bufPos += elementLen;
   }

   *outLen = bufPos;

   return true;
}


 * deserialize a FsckDirInodeList

void SerializationFsck::deserializeFsckDirInodeList(unsigned listElemNum, const char* listStart,
   FsckDirInodeList* outList)
{
   const char* buf = listStart;
   size_t bufPos = 0;
   size_t bufLen = ~0;

   for(unsigned i=0; i < listElemNum; i++)
   {
      FsckDirInode element;
      unsigned elementLen;

      element.deserialize(&buf[bufPos], bufLen-bufPos, &elementLen);

      bufPos += elementLen;

      outList->push_back(element);
   }
}

// =========== serializeFsckChunkList ===========
*
 * Serialization of a FsckChunkList.
 *
 * @return 0 on error, used buffer size otherwise

unsigned SerializationFsck::serializeFsckChunkList(char* buf, FsckChunkList *fsckChunkInodeList)
{
   unsigned listSize = fsckChunkInodeList->size();

   size_t bufPos = 0;

   // elem count info field

   bufPos += serializeUInt(&buf[bufPos], listSize);

   // serialize each element of the list

   FsckChunkListIter iter = fsckChunkInodeList->begin();

   for ( unsigned i = 0; i < listSize; i++, iter++ )
   {
      FsckChunk element = *iter;

      bufPos += element.serialize(&buf[bufPos]);
   }

   return bufPos;
}

*
 * Calculate the needed buffer length for serialization of a FsckChunkList
 *
 * @return needed buffer size

unsigned SerializationFsck::serialLenFsckChunkList(FsckChunkList* fsckChunkInodeList)
{
   unsigned listSize = fsckChunkInodeList->size();

   size_t bufPos = 0;

   bufPos += serialLenUInt();

   FsckChunkListIter iter = fsckChunkInodeList->begin();

   for ( unsigned i = 0; i < listSize; i++, iter++ )
   {
      FsckChunk element = *iter;

      bufPos += element.serialLen();
   }

   return bufPos;
}

*
 * Pre-processes a serialized FsckChunkList for deserialization via deserializeFsckChunkList().
 *
 * @return false on error or inconsistency

bool SerializationFsck::deserializeFsckChunkListPreprocess(const char* buf, size_t bufLen,
   const char** outInfoStart, unsigned *outElemNum, unsigned* outLen)
{
   // Note: This is a fairly inefficient implementation (requiring redundant pre-processing),
   //    but efficiency doesn't matter for the typical use-cases of this method

   size_t bufPos = 0;

   // elem count info field
   unsigned elemNumFieldLen;

   if ( unlikely(!deserializeUInt(&buf[bufPos], bufLen-bufPos, outElemNum,
         &elemNumFieldLen) ) )
      return false;

   bufPos += elemNumFieldLen;

   *outInfoStart = &buf[bufPos];

   for ( unsigned i = 0; i < *outElemNum; i++ )
   {
      FsckChunk element;
      unsigned elementLen;

      if (unlikely(!element.deserialize(&buf[bufPos], bufLen-bufPos, &elementLen)))
         return false;

      bufPos += elementLen;
   }

   *outLen = bufPos;

   return true;
}


 * deserialize a FsckChunkList

void SerializationFsck::deserializeFsckChunkList(unsigned listElemNum, const char* listStart,
   FsckChunkList* outList)
{
   const char* buf = listStart;
   size_t bufPos = 0;
   size_t bufLen = ~0;

   for(unsigned i=0; i < listElemNum; i++)
   {
      FsckChunk element;
      unsigned elementLen;

      element.deserialize(&buf[bufPos], bufLen-bufPos, &elementLen);

      bufPos += elementLen;

      outList->push_back(element);
   }
}

// =========== serializeFsckContDirList ===========
*
 * Serialization of a FsckContDirList.
 *
 * @return 0 on error, used buffer size otherwise

unsigned SerializationFsck::serializeFsckContDirList(char* buf, FsckContDirList *fsckContDirList)
{
   unsigned listSize = fsckContDirList->size();

   size_t bufPos = 0;

   // elem count info field

   bufPos += serializeUInt(&buf[bufPos], listSize);

   // serialize each element of the list

   FsckContDirListIter iter = fsckContDirList->begin();

   for ( unsigned i = 0; i < listSize; i++, iter++ )
   {
      FsckContDir element = *iter;

      bufPos += element.serialize(&buf[bufPos]);
   }

   return bufPos;
}

*
 * Calculate the needed buffer length for serialization of a FsckContDirList
 *
 * @return needed buffer size

unsigned SerializationFsck::serialLenFsckContDirList(FsckContDirList* fsckContDirList)
{
   unsigned listSize = fsckContDirList->size();

   size_t bufPos = 0;

   bufPos += serialLenUInt();

   FsckContDirListIter iter = fsckContDirList->begin();

   for ( unsigned i = 0; i < listSize; i++, iter++ )
   {
      FsckContDir element = *iter;

      bufPos += element.serialLen();
   }

   return bufPos;
}

*
 * Pre-processes a serialized FsckContDirList for deserialization via deserializeFsckContDirList().
 *
 * @return false on error or inconsistency

bool SerializationFsck::deserializeFsckContDirListPreprocess(const char* buf, size_t bufLen,
   const char** outInfoStart, unsigned *outElemNum, unsigned* outLen)
{
   // Note: This is a fairly inefficient implementation (requiring redundant pre-processing),
   //    but efficiency doesn't matter for the typical use-cases of this method

   size_t bufPos = 0;

   // elem count info field
   unsigned elemNumFieldLen;

   if ( unlikely(!deserializeUInt(&buf[bufPos], bufLen-bufPos, outElemNum,
         &elemNumFieldLen) ) )
      return false;

   bufPos += elemNumFieldLen;

   *outInfoStart = &buf[bufPos];

   for ( unsigned i = 0; i < *outElemNum; i++ )
   {
      FsckContDir element;
      unsigned elementLen;

      if (unlikely(!element.deserialize(&buf[bufPos], bufLen-bufPos, &elementLen)))
         return false;

      bufPos += elementLen;
   }

   *outLen = bufPos;

   return true;
}


 * deserialize a FsckContDirList

void SerializationFsck::deserializeFsckContDirList(unsigned listElemNum,
   const char* listStart, FsckContDirList* outList)
{
   const char* buf = listStart;
   size_t bufPos = 0;
   size_t bufLen = ~0;

   for(unsigned i=0; i < listElemNum; i++)
   {
      FsckContDir element;
      unsigned elementLen;

      element.deserialize(&buf[bufPos], bufLen-bufPos, &elementLen);

      bufPos += elementLen;

      outList->push_back(element);
   }
}

// =========== serializeFsckFsIDList ===========
*
 * Serialization of a FsckFsIDList.
 *
 * @return 0 on error, used buffer size otherwise

unsigned SerializationFsck::serializeFsckFsIDList(char* buf, FsckFsIDList *fsckFsIDList)
{
   unsigned listSize = fsckFsIDList->size();

   size_t bufPos = 0;

   // elem count info field

   bufPos += serializeUInt(&buf[bufPos], listSize);

   // serialize each element of the list

   FsckFsIDListIter iter = fsckFsIDList->begin();

   for ( unsigned i = 0; i < listSize; i++, iter++ )
   {
      FsckFsID element = *iter;

      bufPos += element.serialize(&buf[bufPos]);
   }

   return bufPos;
}

*
 * Calculate the needed buffer length for serialization of a FsckFsIDList
 *
 * @return needed buffer size

unsigned SerializationFsck::serialLenFsckFsIDList(FsckFsIDList* fsckFsIDList)
{
   unsigned listSize = fsckFsIDList->size();

   size_t bufPos = 0;

   bufPos += serialLenUInt();

   FsckFsIDListIter iter = fsckFsIDList->begin();

   for ( unsigned i = 0; i < listSize; i++, iter++ )
   {
      FsckFsID element = *iter;

      bufPos += element.serialLen();
   }

   return bufPos;
}

*
 * Pre-processes a serialized FsckDirEntryList for deserialization via
 * deserializeFsckFsIDList().
 *
 * @return false on error or inconsistency

bool SerializationFsck::deserializeFsckFsIDListPreprocess(const char* buf, size_t bufLen,
   const char** outInfoStart, unsigned *outElemNum, unsigned* outLen)
{
   // Note: This is a fairly inefficient implementation (requiring redundant pre-processing),
   //    but efficiency doesn't matter for the typical use-cases of this method

   size_t bufPos = 0;

   // elem count info field
   unsigned elemNumFieldLen;

   if ( unlikely(!deserializeUInt(&buf[bufPos], bufLen-bufPos, outElemNum,
         &elemNumFieldLen) ) )
      return false;

   bufPos += elemNumFieldLen;

   *outInfoStart = &buf[bufPos];

   for ( unsigned i = 0; i < *outElemNum; i++ )
   {
      FsckFsID element;
      unsigned elementLen;

      if (unlikely(!element.deserialize(&buf[bufPos], bufLen-bufPos, &elementLen)))
         return false;

      bufPos += elementLen;
   }

   *outLen = bufPos;

   return true;
}


 * deserialize a FsckFsIDList

void SerializationFsck::deserializeFsckFsIDList(unsigned listElemNum, const char* listStart,
   FsckFsIDList* outList)
{
   const char* buf = listStart;
   size_t bufPos = 0;
   size_t bufLen = ~0;

   for(unsigned i=0; i < listElemNum; i++)
   {
      FsckFsID element;
      unsigned elementLen;

      element.deserialize(&buf[bufPos], bufLen-bufPos, &elementLen);

      bufPos += elementLen;

      outList->push_back(element);
   }
}*/
