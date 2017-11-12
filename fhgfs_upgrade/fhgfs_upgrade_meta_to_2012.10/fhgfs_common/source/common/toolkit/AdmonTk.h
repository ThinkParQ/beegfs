#ifndef ADMONTK_H_
#define ADMONTK_H_

#include <common/Common.h>

struct StorageTargetInfo
{
      uint16_t targetID;
      std::string pathStr;
      int64_t diskSpaceTotal;
      int64_t diskSpaceFree;
};

typedef std::list<StorageTargetInfo> StorageTargetInfoList;
typedef StorageTargetInfoList::iterator StorageTargetInfoListIter;

class AdmonTk
{
   public:
      AdmonTk();
      virtual ~AdmonTk();
};

#endif /* ADMONTK_H_ */
