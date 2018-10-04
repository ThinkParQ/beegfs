#ifndef MODESTORAGERESYNCSTATS_H_
#define MODESTORAGERESYNCSTATS_H_

#include <common/net/message/storage/mirroring/GetStorageResyncStatsMsg.h>
#include <common/net/message/storage/mirroring/GetStorageResyncStatsRespMsg.h>
#include <common/storage/StorageErrors.h>
#include <common/Common.h>
#include <modes/Mode.h>

class ModeResyncStats : public Mode
{
   public:
      ModeResyncStats()
      {
      }

      virtual int execute();

      static void printHelp();

   private:
      NodeType nodeType;

      int getStatsStorage(uint16_t syncToTargetID, bool isBuddyGroupID);
      int getStatsMeta(uint16_t nodeID, bool isBuddyGroupID);
      void printStats(StorageBuddyResyncJobStatistics& jobStats);
      void printStats(MetaBuddyResyncJobStatistics& jobStats);
      void printStats(BuddyResyncJobStatistics jobStats);
      std::string jobStatusToStr(BuddyResyncJobState status);
};


#endif /* MODESTORAGERESYNCSTATS_H_ */
