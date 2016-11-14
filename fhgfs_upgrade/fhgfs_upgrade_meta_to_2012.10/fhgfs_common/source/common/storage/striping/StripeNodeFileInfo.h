#ifndef STRIPENODEFILEINFO_H_
#define STRIPENODEFILEINFO_H_

#include <common/toolkit/MathTk.h>

/*
 * Note: the optimizations here and in other places rely on chunksize being a power of two.
 */

class DynamicFileAttribs
{
   public:
      DynamicFileAttribs() : storageVersion(0)
      {
         // we only need to init storageVersion with 0, because that means all contained attribs
         //    are invalid
      }
      
      DynamicFileAttribs(uint64_t storageVersion, int64_t length,
         int64_t modificationTimeSecs, int64_t lastAccessTimeSecs) :
         storageVersion(storageVersion),
         length(length),
         modificationTimeSecs(modificationTimeSecs),
         lastAccessTimeSecs(lastAccessTimeSecs)
      {
         // nothing to do here (all done in initializer list)
      }
      
      
      uint64_t storageVersion; // use 0 as initial version and then only monotonic increase

      int64_t length;
      int64_t modificationTimeSecs;
      int64_t lastAccessTimeSecs;
};



class StripeNodeFileInfo
{
   public:
      /**
       * @param chunkSize must be a power of two
       * @param chunkSizeLog2 MathTk::log2(chunkSize), just provided (and not computed internally)
       * because the caller probably needs to compute this value anyways and/or can re-use it.
       */
      StripeNodeFileInfo(unsigned chunkSize, unsigned chunkSizeLog2,
         const DynamicFileAttribs& dynAttribs) :
         dynAttribs(dynAttribs)
      {
         this->chunkSize = chunkSize;
         this->chunkSizeLog2 = chunkSizeLog2;
         
         this->numCompleteChunks = dynAttribs.length >> chunkSizeLog2;
      }

      
   private:
      DynamicFileAttribs dynAttribs;
      
      unsigned chunkSize; // must be a power of 2, because we rely on that with our optimizations
      unsigned chunkSizeLog2; // for bitshifting instead of division
      int64_t numCompleteChunks;
      
      
      void applyNewDynAttribs(const DynamicFileAttribs& newAttribs)
      {
         this->dynAttribs = newAttribs;

         this->numCompleteChunks = dynAttribs.length >> chunkSizeLog2;
      }

      
   public:
      // getters & setters
      
      uint64_t getStorageVersion() const
      {
         return dynAttribs.storageVersion;
      }

      int64_t getLength() const
      {
         return dynAttribs.length;
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
            applyNewDynAttribs(newAttribs);
            
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

         int64_t incompleteChunk = (dynAttribs.length & (chunkSize-1) ) ? 1 : 0;
         
         return numCompleteChunks + incompleteChunk;
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

typedef std::vector<DynamicFileAttribs> DynamicFileAttribsVec;
typedef DynamicFileAttribsVec::iterator DynamicFileAttribsVecIter;
typedef DynamicFileAttribsVec::const_iterator DynamicFileAttribsVecCIter;

typedef std::vector<StripeNodeFileInfo> StripeNodeFileInfoVec;
typedef StripeNodeFileInfoVec::iterator StripeNodeFileInfoVecIter;
typedef StripeNodeFileInfoVec::const_iterator StripeNodeFileInfoVecCIter;

#endif /*STRIPENODEFILEINFO_H_*/
