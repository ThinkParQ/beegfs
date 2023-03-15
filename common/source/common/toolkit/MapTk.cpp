#include <common/toolkit/TempFileTk.h>
#include "MapTk.h"

#include <fstream>

/**
 * Splits a line into param and value (divided by the '='-char) and adds it to
 * the map.
 */
void MapTk::addLineToStringMap(std::string line, StringMap* outMap)
{
   std::string::size_type divPos = line.find_first_of("=", 0);

   if(divPos == std::string::npos)
   {
      stringMapRedefine(StringTk::trim(line), "", outMap);
      return;
   }

   std::string param = line.substr(0, divPos);
   std::string value = line.substr(divPos + 1);

   stringMapRedefine(StringTk::trim(param), StringTk::trim(value), outMap);
}

void MapTk::loadStringMapFromFile(const char* filename, StringMap* outMap)
{
   std::ifstream fis(filename);
   if(!fis.is_open() || fis.fail() )
   {
      throw InvalidConfigException(
         std::string("Failed to load map file: ") + filename);
   }

   while(!fis.eof() && !fis.fail() )
   {
      std::string line;

      std::getline(fis, line);
      std::string trimmedLine = StringTk::trim(line);
      if(trimmedLine.length() && (trimmedLine[0] != STORAGETK_FILE_COMMENT_CHAR) )
         addLineToStringMap(trimmedLine, outMap);
   }

   fis.close();
}

/**
 * Note: Saves to a tmp file first and then renames the tmp file.
 * Note: Has a special handling for keys starting with STORAGETK_FILE_COMMENT_CHAR.
 */
void MapTk::saveStringMapToFile(const char* filename, StringMap* map)
{
   std::stringstream out;

   for(StringMapCIter iter = map->begin(); (iter != map->end() ) && !out.fail(); iter++)
   {
      if(!iter->first.empty() && (iter->first[0] != STORAGETK_FILE_COMMENT_CHAR) )
         out << iter->first << "=" << iter->second << std::endl;
      else
         out << iter->first << std::endl; // only "iter->first" for comment or empty lines
   }

   if(out.fail() )
   {
      throw InvalidConfigException(
         std::string("Failed to build data buffer for ") + filename);
   }

   size_t outLen = out.str().length();

   if(TempFileTk::storeTmpAndMove(filename, out.str().c_str(), outLen) != FhgfsOpsErr_SUCCESS)
   {
      throw InvalidConfigException(
         std::string("Unable to write data to file ") + filename);
   }
}

void MapTk::copyUInt64VectorMap(std::map<uint64_t, UInt64Vector*> &inMap,
   std::map<uint64_t, UInt64Vector*> &outMap)
{
   std::map<uint64_t, UInt64Vector*>::iterator mapIter = inMap.begin();

   for ( ; mapIter != inMap.end(); mapIter++)
   {
      UInt64Vector* vector = new UInt64Vector();

      for (UInt64VectorIter vectorIter = mapIter->second->begin();
         vectorIter != mapIter->second->end(); vectorIter++)
      {
         uint64_t value = *vectorIter;
         vector->push_back(value);
      }

      outMap.insert(std::pair<uint64_t, UInt64Vector*>(mapIter->first ,vector));
   }
}
