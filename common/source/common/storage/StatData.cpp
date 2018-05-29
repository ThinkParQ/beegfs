/*
 * Information provided by stat()
 */

#include <common/toolkit/serialization/Serialization.h>
#include <common/app/log/LogContext.h>
#include <common/storage/StatData.h>

/* storing the actual number of blocks means some overhead for us, so define grace blocks for the
 * actual vs. caculated number of blocks. */
#define STATDATA_SPARSE_GRACEBLOCKS        (8)


/**
 * Update values relevant for FileInodes only.
 *
 * Note: This version is compatible with sparse files.
 */
void StatData::updateDynamicFileAttribs(ChunkFileInfoVec& fileInfoVec, StripePattern* stripePattern)
{
   // we use the dynamic attribs only if they have been initialized (=> storageVersion != 0)

   // dynamic attrib algo:
   // walk over all the stripeNodes and...
   // - find last node with max number of (possibly incomplete) chunks
   // - find the latest modification- and lastAccessTimeSecs


   // scan for an initialized index...
   /* note: we assume that in most cases, the user will stat not-opened files (which have
      all storageVersions set to 0), so we optimize for that case. */

   unsigned numValidDynAttribs = 0;

   size_t numStripeTargets = stripePattern->getNumStripeTargetIDs();

   for (size_t i=0; i < fileInfoVec.size(); i++)
   {
      if (fileInfoVec[i].getStorageVersion() )
      {
         numValidDynAttribs++;
      }
   }

   if (numValidDynAttribs == 0)
   { // no valid dyn attrib element found => use static attribs and nothing to do
      return;
   }

   /* we found a valid dyn attrib index => start with first target anyways to avoid false file
      length computations... */

   unsigned lastMaxNumChunksIndex = 0;
   int64_t maxNumChunks = fileInfoVec[0].getNumChunks();

   // the first mtime on the metadata server can be newer then the mtime on the storage server due
   // to minor time difference between the servers, so the ctime should be updated after every
   // close, this issue should be fixed by the usage of the max(...) function
   // note: only use max(MDS, first chunk) if we don't have information from all chunks, because
   // if we have information from all chunks and the info on the MDS differs, info on the MDS must
   // be wrong and therefore should be overwritten
   int64_t newModificationTimeSecs;
   int64_t newLastAccessTimeSecs;
   if (numValidDynAttribs == numStripeTargets)
   {
      newModificationTimeSecs = fileInfoVec[0].getModificationTimeSecs();

      newLastAccessTimeSecs = fileInfoVec[0].getLastAccessTimeSecs();
   }
   else
   {
      newModificationTimeSecs = std::max(fileInfoVec[0].getModificationTimeSecs(),
         settableFileAttribs.modificationTimeSecs);

      newLastAccessTimeSecs = std::max(fileInfoVec[0].getLastAccessTimeSecs(),
         settableFileAttribs.lastAccessTimeSecs);
   }

   setTargetChunkBlocks(0, fileInfoVec[0].getNumBlocks(), numStripeTargets);

   int64_t newFileSize;

   for (unsigned target = 1; target < fileInfoVec.size(); target++)
   {
      /* note: we cannot ignore storageVersion==0 here, because that would lead to wrong file
       *       length computations.
       * note2: fileInfoVec will be initialized once we have read meta data from disk, so if a
       *        storage target does not answer or no request was sent to it,
       *        we can still use old data from fileInfoVec
       *
       * */

      int64_t currentNumChunks = fileInfoVec[target].getNumChunks();

      if(currentNumChunks >= maxNumChunks)
      {
         lastMaxNumChunksIndex = target;
         maxNumChunks = currentNumChunks;
      }

      int64_t chunkModificationTimeSecs = fileInfoVec[target].getModificationTimeSecs();
      if(chunkModificationTimeSecs > newModificationTimeSecs)
         newModificationTimeSecs = chunkModificationTimeSecs;

      int64_t currentLastAccessTimeSecs = fileInfoVec[target].getLastAccessTimeSecs();
      if(currentLastAccessTimeSecs > newLastAccessTimeSecs)
         newLastAccessTimeSecs = currentLastAccessTimeSecs;

      setTargetChunkBlocks(target, fileInfoVec[target].getNumBlocks(), numStripeTargets);
   }

   if(maxNumChunks)
   { // we have chunks, so "fileLength > 0"

      // note: we do all this complex filesize calculation stuff here to support sparse files

      // every target before lastMax-target must have the same number of (complete) chunks
      int64_t lengthBeforeLastMaxIndex = lastMaxNumChunksIndex * maxNumChunks *
         stripePattern->getChunkSize();

      // every target after lastMax-target must have exactly one chunk less than the lastMax-target
      int64_t lengthAfterLastMaxIndex = (fileInfoVec.size() - lastMaxNumChunksIndex - 1) *
         (maxNumChunks - 1) * stripePattern->getChunkSize();

      // all the prior calcs are based on the lastMax-target size, so we can simply use that one
      int64_t lengthLastMaxNode = fileInfoVec[lastMaxNumChunksIndex].getFileSize();

      int64_t totalLength = lengthBeforeLastMaxIndex + lengthAfterLastMaxIndex + lengthLastMaxNode;

      newFileSize = totalLength;

      uint64_t calcBlocks = newFileSize >> BLOCK_SHIFT;
      uint64_t usedBlockSum = getNumBlocks();

      if (usedBlockSum + STATDATA_SPARSE_GRACEBLOCKS < calcBlocks)
         setSparseFlag();
      else
         unsetSparseFlag();

   }
   else
   { // no chunks, no filesize
      newFileSize = 0;
   }


   // now update class values

   setLastAccessTimeSecs(newLastAccessTimeSecs);

   // mtime or fileSize updated, so also ctime needs to be updated
   if (newModificationTimeSecs != getModificationTimeSecs() ||
         newFileSize != getFileSize())
      setAttribChangeTimeSecs(TimeAbs().getTimeval()->tv_sec);

   setModificationTimeSecs(newModificationTimeSecs);
   setFileSize(newFileSize);
}

bool StatData::operator==(const StatData& second) const
{
   return flags == second.flags
      && settableFileAttribs == second.settableFileAttribs
      && chunkBlocksVec == second.chunkBlocksVec
      && creationTimeSecs == second.creationTimeSecs
      && attribChangeTimeSecs == second.attribChangeTimeSecs
      && fileSize == second.fileSize
      && nlink == second.nlink
      && contentsVersion == second.contentsVersion;
}
