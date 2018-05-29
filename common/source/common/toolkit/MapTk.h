#ifndef MAPTK_H_
#define MAPTK_H_

#include <common/Common.h>
#include <common/toolkit/StorageTk.h>
#include <common/app/config/InvalidConfigException.h>



class MapTk
{
   public:
      static void addLineToStringMap(std::string line, StringMap* outMap);
      static void loadStringMapFromFile(const char* filename, StringMap* outMap);
      static void saveStringMapToFile(const char* filename, StringMap* map);
      static void copyUInt64VectorMap(std::map<uint64_t, UInt64Vector*> &inMap,
         std::map<uint64_t, UInt64Vector*> &outMap);

   private:
      MapTk() {}

   public:
      // inliners
      static void stringMapRedefine(std::string keyStr, std::string valueStr, StringMap* outMap)
      {
         outMap->erase(keyStr);

         outMap->insert(StringMapVal(keyStr, valueStr) );
      }

};

#endif /* MAPTK_H_ */
