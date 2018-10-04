#ifndef TRUNCLOCALFILEMSGEX_H_
#define TRUNCLOCALFILEMSGEX_H_

#include <common/net/message/storage/TruncLocalFileMsg.h>
#include <common/storage/StorageErrors.h>
#include <common/storage/Path.h>

class StorageTarget;

class TruncLocalFileMsgEx : public TruncLocalFileMsg
{
   private:
      struct DynamicAttribs
      {
         DynamicAttribs() : filesize(0), allocedBlocks(0), modificationTimeSecs(0),
            lastAccessTimeSecs(0), storageVersion(0) {}

         int64_t filesize;
         int64_t allocedBlocks; // allocated 512byte blocks (relevant for sparse files)
         int64_t modificationTimeSecs;
         int64_t lastAccessTimeSecs;
         uint64_t storageVersion;
      };

   public:
      virtual bool processIncoming(ResponseContext& ctx);

   private:
      FhgfsOpsErr truncFile(uint16_t targetId, int targetFD, const Path* chunkDirPath,
         const std::string& chunkFilePathStr, std::string entryID, bool hasOrigFeature);
      int getTargetFD(const StorageTarget& target, ResponseContext& ctx, bool* outResponseSent);
      bool getDynamicAttribsByPath(const int dirFD, const char* path, uint16_t targetID,
         std::string fileID, DynamicAttribs& outDynAttribs);
      bool getFakeDynAttribs(uint16_t targetID, std::string fileID, DynamicAttribs& outDynAttribs);
      FhgfsOpsErr forwardToSecondary(StorageTarget& target, ResponseContext& ctx,
            bool* outChunkLocked);
};

#endif /*TRUNCLOCALFILEMSGEX_H_*/
