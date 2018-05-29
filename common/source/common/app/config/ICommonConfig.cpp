#include <common/toolkit/StringTk.h>
#include <common/toolkit/StorageTk.h>
#include "ICommonConfig.h"

/**
 * Loads a file into a string list (line by line).
 *
 * Loaded strings are trimmed before they are added to the list. Empty lines  and lines starting
 * with STORAGETK_FILE_COMMENT_CHAR are not added to the list.
 */
void ICommonConfig::loadStringListFile(const char* filename, StringList& outList)
{
   std::ifstream fis(filename);
   if(!fis.is_open() || fis.fail() )
   {
      throw InvalidConfigException(
         std::string("Failed to open file: ") + filename);
   }

   while(!fis.eof() && !fis.fail() )
   {
      std::string line;

      std::getline(fis, line);
      std::string trimmedLine = StringTk::trim(line);
      if(trimmedLine.length() && (trimmedLine[0] != STORAGETK_FILE_COMMENT_CHAR) )
         outList.push_back(trimmedLine);
   }

   fis.close();
}





