#ifndef CHUNKFILEINFO_H_
#define CHUNKFILEINFO_H_

#include <common/toolkit/MathTk.h>

#include "DynamicFileAttribs.h"


class ChunkFileInfo;
typedef std::vector<ChunkFileInfo> ChunkFileInfoVec;
typedef ChunkFileInfoVec::iterator ChunkFileInfoVecIter;
typedef ChunkFileInfoVec::const_iterator ChunkFileInfoVecCIter;


/*
 * Note: the optimizations here and in other places rely on chunksize being a power of two.
 */


class ChunkFileInfo
{
   public:
      /**
       * @param chunkSize must be a power of two
       * @param chunkSizeLog2 MathTk::log2(chunkSize), just provided (and not computed internally)
       * because the caller probably needs to compute this value anyways and/or can re-use it.
       */
      ChunkFileInfo(unsigned chunkSize, unsigned chunkSizeLog2,
         const DynamicFileAttribs& dynAttribs) :
         dynAttribs(dynAttribs)
      {
         this->chunkSize = chunkSize;
         this->chunkSizeLog2 = chunkSizeLog2;

         this->numCompleteChunks = dynAttribs.fileSize >> chunkSizeLog2;
      }

      /**
       * For dezerialisation only
       */
      ChunkFileInfo() {};

      bool operator==(const ChunkFileInfo& other) const;

      bool operator!=(const ChunkFileInfo& other) const { return !(*this == other); }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->dynAttribs
            % obj->chunkSize
            % obj->chunkSizeLog2
            % obj->numCompleteChunks;
      }

   private:
      DynamicFileAttribs dynAttribs;

      uint32_t chunkSize; // must be a power of 2, because we rely on that with our optimizations
      uint32_t chunkSizeLog2; // for bitshifting instead of division
      int64_t numCompleteChunks;


   public:
      // getters & setters

      uint64_t getStorageVersion() const
      {
         return dynAttribs.storageVersion;
      }

      int64_t getFileSize() const
      {
         return dynAttribs.fileSize;
      }

      int64_t getModificationTimeSecs() const
      {
         return dynAttribs.modificationTimeSecs;
      }

      int64_t getLastAccessTimeSecs() const
      {
         return dynAttribs.lastAccessTimeSecs;
      }


      /**
       * Note: Only applies the new data if the storageVersion has increased (relies on monotonic
       * increasing storageVersions)
       *
       * @return true if attribs updated (because of higher storageVersion)
       */
      bool updateDynAttribs(const DynamicFileAttribs& newAttribs)
      {
         // note: we do a ">"-comparison, so newAttribs->storageVersion==0 will not be accepted
         //    (=> zero means invalid/uninitialized version)

         // we only apply the data if the storageVersion is higher than current
         if(newAttribs.storageVersion > dynAttribs.storageVersion)
         {
            this->dynAttribs = newAttribs;

            this->numCompleteChunks = dynAttribs.fileSize >> chunkSizeLog2;

            return true;
         }

         return false;
      }

      int64_t getNumCompleteChunks() const
      {
         return numCompleteChunks;
      }

      int64_t getNumChunks() const
      {
         /* note: chunkSize is a power of two, so "(dynAttribs.length & (chunkSize-1) )" equals
            "(dynAttribs.length % chunkSize)". */

         int64_t incompleteChunk = (dynAttribs.fileSize & (chunkSize-1) ) ? 1 : 0;

         return numCompleteChunks + incompleteChunk;
      }

      uint64_t getNumBlocks() const
      {
         return dynAttribs.numBlocks;
      }

      /**
       * Note: This should only be used in very special cases (e.g. forced updates), because caller
       * potentially ignores storageVersion check.
       */
      DynamicFileAttribs* getRawDynAttribs()
      {
         return &dynAttribs;
      }
};

#endif /*CHUNKFILEINFO_H_*/
