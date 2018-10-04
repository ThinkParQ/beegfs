#ifndef MGMTD_TARGETMAPPEREX
#define MGMTD_TARGETMAPPEREX

#include <common/nodes/TargetMapper.h>

class TargetMapperEx : public TargetMapper
{
   public:
      bool loadFromFile();
      bool saveToFile();

      void setStorePath(std::string storePath)
      {
         this->storePath = storePath;
      }

   private:
      void exportToStringMap(StringMap& outExportMap) const;
      void importFromStringMap(StringMap& importMap);

      std::string storePath; // set to enable load/save methods (setting is not thread-safe)
};

#endif
