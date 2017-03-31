#ifndef MGMTD_STORAGETKEX_H_
#define MGMTD_STORAGETKEX_H_

#include <app/config/Config.h>
#include <common/Common.h>
#include <common/storage/Path.h>
#include <common/toolkit/StorageTk.h>


#define STORAGETK_FORMAT_MIN_VERSION        2
#define STORAGETK_FORMAT_SHORTS_NODE_IDS    3
#define STORAGETK_FORMAT_LONG_NODE_IDS      4
#define STORAGETK_FORMAT_STATES_META        "nodeStates"
#define STORAGETK_FORMAT_STATES_STORAGE     "targetStates"



class StorageTkEx
{
   public:
      static bool createStorageFormatFile(const std::string pathStr, int formatVersion,
            StringMap* formatProperties = nullptr);
      static void updateStorageFormatFile(const std::string pathStr, int minVersion,
            int currentVersion, StringMap* inOutFormatProperties);

   private:
      StorageTkEx() {}
};

#endif /* STORAGETKEX_H_ */
