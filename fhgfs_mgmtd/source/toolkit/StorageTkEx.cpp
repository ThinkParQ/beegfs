#include "StorageTkEx.h"

bool StorageTkEx::createStorageFormatFile(const std::string pathStr, int formatVersion,
      StringMap* formatProperties)
{
   return StorageTk::createStorageFormatFile(pathStr, formatVersion, formatProperties);
}

/**
 * Note: Contrary to the checkAndUpdateStorageFormatFile in "common", this one treats
 * formatProperties as an *in* parameter instead of an *out* parameter.
 * @param pathStr path to the main storage working directory (not including a filename)
 * @param formatProperties key/value pairs to be stored in the format file.
 * @throws exception if format file was not valid (eg didn't exist or contained wrong version).
 */
void StorageTkEx::updateStorageFormatFile(const std::string pathStr, int minVersion,
   int currentVersion, StringMap* formatProperties)
{
   // update the version
   Path dirPath(pathStr);
   Path filePath = dirPath / STORAGETK_FORMAT_FILENAME;
   std::string filePathStr = filePath.str();
   std::string backupFilePathString = filePathStr + STORAGETK_BACKUP_EXT;

   int renameRes = ::rename(filePathStr.c_str(), backupFilePathString.c_str() );
   if (renameRes)
   {
      throw InvalidConfigException(
         std::string("Failed to create " STORAGETK_FORMAT_FILENAME " backup file: ") +
         System::getErrString(errno) + ".");
   }

   bool createSuccess = createStorageFormatFile(pathStr, currentVersion, formatProperties);
   if (!createSuccess)
   {
      int renameRes = ::rename(backupFilePathString.c_str(), filePathStr.c_str() );
      if (renameRes)
      {
         throw InvalidConfigException(
            std::string("Failed to rename back " STORAGETK_FORMAT_FILENAME " backup file: ") +
            System::getErrString(errno) + ".");
      }

      throw InvalidConfigException(
         std::string("Failed to update " STORAGETK_FORMAT_FILENAME "!") );
   }
   else
   {
      int unlinkRes = ::unlink(backupFilePathString.c_str() );
      if (unlinkRes)
      {
         throw InvalidConfigException(
            std::string("Failed to unlink: ") + backupFilePathString + ": " +
            System::getErrString(errno) + ".");
      }
   }
}

