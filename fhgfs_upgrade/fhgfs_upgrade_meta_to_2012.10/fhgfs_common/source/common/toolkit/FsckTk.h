#include <common/Common.h>
#include <common/fsck/FsckDirEntry.h>
#include <common/fsck/FsckFileInode.h>
#include <common/storage/StorageDefinitions.h>
#include <common/storage/EntryInfo.h>

#ifndef FSCKTK_H_
#define FSCKTK_H_

enum FsckStripePatternType
{
   FsckStripePatternType_INVALID = 0, FsckStripePatternType_RAID0 = 1
};

class FsckTk
{
   public:
      FsckTk(){ };
      virtual ~FsckTk(){ };

      static FsckStripePatternType stripePatternToFsckStripePattern(StripePattern* stripePattern,
         UInt16Vector* outTargetIDVector = NULL, unsigned *outChunkSize = NULL);
      static StripePattern* FsckStripePatternToStripePattern(UInt16Vector* targetIDVector,
         FsckStripePatternType stripePatternType, unsigned chunkSize);

      static FsckDirEntryType DirEntryTypeToFsckDirEntryType(DirEntryType entryType);

   private:

   public:
      template<typename T>
      static bool listsEqual(std::list<T>* listA, std::list<T>* listB)
      {
         // check sizes
         if ( listA->size() != listB->size() )
            return false;

         // sort both lists and compare elements
         listA->sort();
         listB->sort();

         typename std::list<T>::iterator iterA = listA->begin();
         typename std::list<T>::iterator iterB = listB->begin();

         while ( iterA != listA->end() )
         {
            T objectA = *iterA;
            T objectB = *iterB;

            if ( objectA != objectB )
               return false;

            iterA++;
            iterB++;
         }

         return true;
      }
};

#endif /* FSCKTK_H_ */
