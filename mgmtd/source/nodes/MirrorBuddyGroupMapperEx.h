#ifndef MIRRORBUDDYGROUPMAPPEREX_H_
#define MIRRORBUDDYGROUPMAPPEREX_H_

#include <common/nodes/MirrorBuddyGroupMapper.h>

class MirrorBuddyGroupMapperEx : public MirrorBuddyGroupMapper
{
   friend class MgmtdTargetStateStore;

   public:
      MirrorBuddyGroupMapperEx(std::string storePath):
         storePath(std::move(storePath))
      {
      }

      MirrorBuddyGroupMapperEx(std::string storePath, TargetMapper* targetMapper):
         MirrorBuddyGroupMapper(targetMapper),
         storePath(std::move(storePath))
      {
      }

      bool loadFromFile();
      bool saveToFile();

   private:
      std::string storePath;

      void switchover(uint16_t buddyGroupID);

      void exportToStringMap(StringMap& outExportMap) const;
      void importFromStringMap(StringMap& importMap);
};

#endif
