#ifndef SERIALIZATIONFSCK_H_
#define SERIALIZATIONFSCK_H_

#include <common/Common.h>

#include <common/fsck/FsckChunk.h>
#include <common/fsck/FsckContDir.h>
#include <common/fsck/FsckDirEntry.h>
#include <common/fsck/FsckDirInode.h>
#include <common/fsck/FsckFsID.h>
#include <common/toolkit/serialization/Serialization.h>

/*
 * Serialization methods for fsck
 */
class SerializationFsck
{
   public:
      SerializationFsck();
      virtual ~SerializationFsck();

      // FsckDirEntryList (de)serialization
      static unsigned serializeFsckDirEntryList(char* buf, FsckDirEntryList *fsckDirEntryList);
      static unsigned serialLenFsckDirEntryList(FsckDirEntryList* fsckDirEntryList);
      static bool deserializeFsckDirEntryListPreprocess(const char* buf, size_t bufLen,
         const char** outInfoStart, unsigned *outElemNum, unsigned* outListBufLen);
      static void deserializeFsckDirEntryList(unsigned fsckDirEntryListElemNum,
         const char* fsckDirEntryListStart, FsckDirEntryList* outList);

      // FsckFileInodeList (de)serialization
      static unsigned serializeFsckFileInodeList(char* buf, FsckFileInodeList *fsckFileInodeList);
      static unsigned serialLenFsckFileInodeList(FsckFileInodeList* fsckFileInodeList);
      static bool deserializeFsckFileInodeListPreprocess(const char* buf, size_t bufLen,
         const char** outInfoStart, unsigned *outElemNum, unsigned* outListBufLen);
      static void deserializeFsckFileInodeList(unsigned fsckFileInodeListElemNum,
         const char* fsckFileInodeListStart, FsckFileInodeList* outList);

      // FsckDirInodeList (de)serialization
      static unsigned serializeFsckDirInodeList(char* buf, FsckDirInodeList *fsckDirInodeList);
      static unsigned serialLenFsckDirInodeList(FsckDirInodeList* fsckDirInodeList);
      static bool deserializeFsckDirInodeListPreprocess(const char* buf, size_t bufLen,
         const char** outInfoStart, unsigned *outElemNum, unsigned* outListBufLen);
      static void deserializeFsckDirInodeList(unsigned fsckDirInodeListElemNum,
         const char* fsckDirInodeListStart, FsckDirInodeList* outList);

      // FsckChunkList (de)serialization
      static unsigned serializeFsckChunkList(char* buf, FsckChunkList *fsckChunkInodeList);
      static unsigned serialLenFsckChunkList(FsckChunkList* fsckChunkInodeList);
      static bool deserializeFsckChunkListPreprocess(const char* buf, size_t bufLen,
         const char** outInfoStart, unsigned *outElemNum, unsigned* outListBufLen);
      static void deserializeFsckChunkList(unsigned fsckChunkInodeListElemNum,
         const char* fsckChunkInodeListStart, FsckChunkList* outList);

      // FsckContDirList (de)serialization
      static unsigned serializeFsckContDirList(char* buf, FsckContDirList *fsckContDirList);
      static unsigned serialLenFsckContDirList(FsckContDirList* fsckContDirList);
      static bool deserializeFsckContDirListPreprocess(const char* buf, size_t bufLen,
         const char** outInfoStart, unsigned *outElemNum, unsigned* outListBufLen);
      static void deserializeFsckContDirList(unsigned fsckContDirListElemNum,
         const char* fsckContDirListStart, FsckContDirList* outList);

      // FsckFsIDList (de)serialization
      static unsigned serializeFsckFsIDList(char* buf, FsckFsIDList *fsckFsIDList);
      static unsigned serialLenFsckFsIDList(FsckFsIDList* fsckContDirList);
      static bool deserializeFsckFsIDListPreprocess(const char* buf, size_t bufLen,
         const char** outInfoStart, unsigned *outElemNum, unsigned* outListBufLen);
      static void deserializeFsckFsIDList(unsigned listElemNum, const char* listStart,
         FsckFsIDList* outList);

   public:
      /**
       * Serialization of an Fsck....-object list
       *
       * @return 0 on error, used buffer size otherwise
       */
      template <typename FsckObject>
      static unsigned serializeFsckObjectList(char* buf, std::list<FsckObject>* list)
      {
         unsigned listSize = list->size();

         size_t bufPos = 0;

         // elem count info field

         bufPos += Serialization::serializeUInt(&buf[bufPos], listSize);

         // serialize each element of the list

         typename std::list<FsckObject>::iterator iter = list->begin();

         for ( unsigned i = 0; i < listSize; i++, iter++ )
         {
            FsckObject element = *iter;

            bufPos += element.serialize(&buf[bufPos]);
         }

         return bufPos;
      }


      /**
       * Calculate the needed buffer length for serialization of a Fsck....-object list
       *
       * @return needed buffer size
       */
      template <typename FsckObject>
      static unsigned serialLenFsckObjectList(std::list<FsckObject>* list)
      {
         unsigned listSize = list->size();

         size_t bufPos = 0;

         bufPos += Serialization::serialLenUInt();

         typename std::list<FsckObject>::iterator iter = list->begin();

         for ( unsigned i = 0; i < listSize; i++, iter++ )
         {
            FsckObject element = *iter;

            bufPos += element.serialLen();
         }

         return bufPos;
      }

      /**
       * Pre-processes a serialized Fsck...-object list  for deserialization via
       * deserializeFsckObjectList().
       *
       * @return false on error or inconsistency
       */
      template <typename FsckObject>
      static bool deserializeFsckObjectListPreprocess(const char* buf, size_t bufLen,
         const char** outInfoStart, unsigned *outElemNum, unsigned* outLen)
      {
         // Note: This is a fairly inefficient implementation (requiring redundant pre-processing),
         //    but efficiency doesn't matter for the typical use-cases of this method

         size_t bufPos = 0;

         // elem count info field
         unsigned elemNumFieldLen;

         if ( unlikely(!Serialization::deserializeUInt(&buf[bufPos], bufLen-bufPos, outElemNum,
               &elemNumFieldLen) ) )
            return false;

         bufPos += elemNumFieldLen;

         *outInfoStart = &buf[bufPos];

         for ( unsigned i = 0; i < *outElemNum; i++ )
         {
            FsckObject element;
            unsigned elementLen;

            if (unlikely(!element.deserialize(&buf[bufPos], bufLen-bufPos, &elementLen)))
               return false;

            bufPos += elementLen;
         }

         *outLen = bufPos;

         return true;
      }

      /*
       * deserialize a Fsck....-object list
       */
      template <typename FsckObject>
      static void deserializeFsckObjectList(unsigned listElemNum, const char* listStart,
         std::list<FsckObject>* outList)
      {
         const char* buf = listStart;
         size_t bufPos = 0;
         size_t bufLen = ~0;

         for(unsigned i=0; i < listElemNum; i++)
         {
            FsckObject element;
            unsigned elementLen;

            element.deserialize(&buf[bufPos], bufLen-bufPos, &elementLen);

            bufPos += elementLen;

            outList->push_back(element);
         }
      }
};

#endif /* SERIALIZATIONFSCK_H_ */
