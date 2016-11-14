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

bool MapTk::loadStringMapFromFile(const char* filename, StringMap* outMap)
{
   char line[STORAGETK_FILE_MAX_LINE_LENGTH];

   std::ifstream fis(filename);
   if(!fis.is_open() || fis.fail() )
   {
      std::cout << "Failed to load map file: " << filename;
      return false;
   }

   while(!fis.eof() && !fis.fail() )
   {
      fis.getline(line, STORAGETK_FILE_MAX_LINE_LENGTH);
      std::string trimmedLine = StringTk::trim(line);
      if(trimmedLine.length() && (trimmedLine[0] != STORAGETK_FILE_COMMENT_CHAR) )
         addLineToStringMap(trimmedLine, outMap);
   }

   fis.close();

   return true;
}

bool MapTk::saveStringMapToFile(const char* filename, StringMap* map)
{
   std::string line;

   std::ofstream file(filename, std::ios_base::out | std::ios_base::in | std::ios_base::trunc);
   if(!file.is_open() || file.fail() )
   {
      std::cout << "Failed to open map file for writing: " << filename;
      return false;
   }

   for(StringMapCIter iter = map->begin();
       (iter != map->end() ) && !file.fail();
       iter++)
   {
      if(!iter->first.empty() && !iter->second.empty() &&
         (iter->first[0] != STORAGETK_FILE_COMMENT_CHAR) )
      {
         file << iter->first << "=" << iter->second << std::endl;
      }
      else
         file << iter->first << std::endl; // only "iter->first" for comment or empty lines
   }

   if(file.fail() )
   {
      file.close();

      std::cout << "Failed to save data to map file: " << filename;
      return false;
   }

   file.close();

   return true;
}

