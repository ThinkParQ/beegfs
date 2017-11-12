#ifndef DENTRYINODEMETADATA_H_
#define DENTRYINODEMETADATA_H_

#include <common/storage/striping/StripePattern.h>
#include <common/storage/Metadata.h>
#include <common/storage/StatData.h>


/* inode data inlined into a direntry, such as in DIRENTRY_STORAGE_FORMAT_VER3 */
class DentryInodeMeta
{
   public:

      DentryInodeMeta()
      {
         this->stripePattern              = NULL;
         this->dentryFeatureFlags         = 0;
         this->inodeFeatureFlags          = 0;
         this->deletePatternOnDestruction = true;
         this->chunkHash                  = 0;
      }

      DentryInodeMeta(StripePattern* stripePattern, std::string& entryID, StatData* statData,
         unsigned chunkHash)
      {
         this->stripePattern              = stripePattern->clone();
         this->entryID                    = entryID;
         this->inodeStatData              = *statData;
         this->dentryFeatureFlags         = 0; // unknown at this point
         this->inodeFeatureFlags          = 0;
         this->deletePatternOnDestruction = true;
         this->chunkHash                  = chunkHash;
}

      ~DentryInodeMeta()
      {
         if (this->deletePatternOnDestruction)
            SAFE_DELETE(this->stripePattern);
      }

   private:
      uint32_t chunkHash;          // hash for storage chunks
      unsigned dentryFeatureFlags; // features of the dentry, included here for convenience
      unsigned inodeFeatureFlags; // feature flags for the inode itself, e.g. for mirroring

      StatData inodeStatData;
      std::string entryID; // filesystem-wide unique string
      StripePattern* stripePattern;

      bool deletePatternOnDestruction;

   public:
      void setDentryFeatureFlags(unsigned flags)
      {
         this->dentryFeatureFlags = flags;
      }

      void addDentryFeatureFlag(unsigned flag)
      {
         this->dentryFeatureFlags |= flag;
      }

      void setInodeFeatureFlags(unsigned flags)
      {
         this->inodeFeatureFlags = flags;
      }

      void addInodeFeatureFlag(unsigned flag)
      {
         this->inodeFeatureFlags |= flag;
      }

      void setInodeStatData(StatData* statData)
      {
         this->inodeStatData = *statData;
      }

      void setID(std::string& entryID)
      {
         this->entryID = entryID;
      }

      void setPattern(StripePattern* pattern)
      {
         this->stripePattern = pattern;
      }

      void setChunkHash(unsigned chunkHash)
      {
         this->chunkHash = chunkHash;
      }

      /**
       * Set several values required to for dentry-serialization with inlined inode, or for inodes
       * in the inode-dir, but in the dentry-format.
       */
      void setSerializationData(std::string entryID, StatData* statData, StripePattern* pattern,
         unsigned chunkHash, unsigned inodeFeatureFlags, unsigned dentryFeatureFlags,
         bool deletePatternOnDestruction)
      {
         this->entryID                    = entryID;
         this->stripePattern              = pattern;
         this->chunkHash                  = chunkHash;
         this->inodeFeatureFlags          = inodeFeatureFlags;
         this->dentryFeatureFlags         = dentryFeatureFlags;
         this->deletePatternOnDestruction = deletePatternOnDestruction;

         this->inodeStatData.set(statData);

      }

      unsigned getDentryFeatureFlags()
      {
         return this->dentryFeatureFlags;
      }

      unsigned getInodeFeatureFlags()
      {
         return this->inodeFeatureFlags;
      }

      StatData* getInodeStatData()
      {
         return &this->inodeStatData;
      }

      std::string getID()
      {
         return this->entryID;
      }

      StripePattern* getPattern()
      {
         return this->stripePattern;
      }

      unsigned getChunkHash()
      {
         return this->chunkHash;
      }

      /**
       * Return the pattern and set the internal pattern to NULL to make sure it does not get
       * deleted on object destruction.
       */
      StripePattern* getPatternAndSetToNull()
      {
         StripePattern* pattern = this->stripePattern;
         this->stripePattern = NULL;

         return pattern;
      }

};


#endif /* DENTRYINODEMETADATA_H_ */
