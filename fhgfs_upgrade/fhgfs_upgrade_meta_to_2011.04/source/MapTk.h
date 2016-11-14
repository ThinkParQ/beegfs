#ifndef MAPTK_H_
#define MAPTK_H_

#include "Common.h"
#include "StorageTk.h"


class MapTk
{
   public:
      static void addLineToStringMap(std::string line, StringMap* outMap);
      static bool loadStringMapFromFile(const char* filename, StringMap* outMap);
      static bool saveStringMapToFile(const char* filename, StringMap* map);

   private:
      MapTk() {}

   public:
      // inliners
      static void stringMapRedefine(std::string keyStr, std::string valueStr, StringMap* outMap)
      {
         if(outMap->find(keyStr) != outMap->end() )
            outMap->erase(keyStr);

         outMap->insert(StringMapVal(keyStr, valueStr) );
      }

};


#endif /* MAPTK_H_ */
