#include <program/Program.h>
#include "StorageTkEx.h"

#include <sys/xattr.h>

#define STORAGETK_FORMAT_XATTR   "xattr"


/**
 * Note: Creates the file only if it does not exist yet.
 *
 * @return true if format file was created or existed already
 */
bool StorageTkEx::createStorageFormatFile(const std::string pathStr)
{
   App* app = Program::getApp();
   Config* cfg = app->getConfig();

   StringMap formatProperties;

   formatProperties[STORAGETK_FORMAT_XATTR] = cfg->getStoreUseExtendedAttribs() ? "true" : "false";

   return StorageTk::createStorageFormatFile(pathStr, STORAGETK_FORMAT_CURRENT_VERSION,
      &formatProperties);
}

/**
 * Compatibility and validity check of storage format file contents.
 *
 * @param pathStr path to the main storage working directory (not including a filename)
 * @throws exception if format file was not valid (eg didn't exist or contained wrong version).
 */
void StorageTkEx::checkStorageFormatFile(const std::string pathStr)
{
   App* app = Program::getApp();
   Config* cfg = app->getConfig();

   StringMap formatProperties;
   StringMapIter formatIter;

   formatProperties = StorageTk::loadAndUpdateStorageFormatFile(pathStr,
         STORAGETK_FORMAT_MIN_VERSION, STORAGETK_FORMAT_CURRENT_VERSION);

   formatIter = formatProperties.find(STORAGETK_FORMAT_XATTR);
   if(formatIter == formatProperties.end() )
   {
      throw InvalidConfigException(std::string("Property missing from storage format file: ") +
         STORAGETK_FORMAT_XATTR " (dir: " + pathStr + ")");
   }

   if(cfg->getStoreUseExtendedAttribs() != StringTk::strToBool(formatIter->second) )
   {
      throw InvalidConfigException("Mismatch of extended attributes settings in storage format file"
         " and daemon config file.");
   }
}

/**
 * Note: intended to be used with fsck only.
 * No locks at all at the moment
 */
FhgfsOpsErr StorageTkEx::getContDirIDsIncremental(unsigned hashDirNum, bool buddyMirrored,
   int64_t lastOffset, unsigned maxOutEntries, StringList* outContDirIDs, int64_t* outNewOffset)
{
   const char* logContext = "StorageTkEx (get cont dir ids inc)";
   App* app = Program::getApp();

   FhgfsOpsErr retVal = FhgfsOpsErr_INTERNAL;
   unsigned numEntries = 0;
   struct dirent* dirEntry = NULL;

   unsigned firstLevelHashDir;
   unsigned secondLevelHashDir;
   StorageTk::splitHashDirs(hashDirNum, &firstLevelHashDir, &secondLevelHashDir);

   const Path* dentriesPath =
      buddyMirrored ? app->getBuddyMirrorDentriesPath() : app->getDentriesPath();

   std::string path = StorageTkEx::getMetaDentriesHashDir(dentriesPath->str(),
      firstLevelHashDir, secondLevelHashDir);

   DIR* dirHandle = opendir(path.c_str() );
   if(!dirHandle)
   {
      LogContext(logContext).logErr(std::string("Unable to open dentries directory: ") +
         path + ". SysErr: " + System::getErrString() );

      goto return_res;
   }


   errno = 0; // recommended by posix (readdir(3p) )

   // seek to offset
   seekdir(dirHandle, lastOffset); // (seekdir has no return value)

   // the actual entry reading
   while ( (numEntries < maxOutEntries) && (dirEntry = StorageTk::readdirFiltered(dirHandle)) )
   {
      std::string dirName = dirEntry->d_name;
      std::string dirID = dirName.substr(0, dirName.length() );

      *outNewOffset = dirEntry->d_off;

      // skip root dir if this is not the root MDS
      NumNodeID rootNodeNumID = Program::getApp()->getMetaRoot().getID();
      NumNodeID localNodeNumID = buddyMirrored
         ? NumNodeID(Program::getApp()->getMetaBuddyGroupMapper()->getLocalGroupID())
         : Program::getApp()->getLocalNode().getNumID();
      if (unlikely ( (dirID.compare(META_ROOTDIR_ID_STR) == 0) &&
         (localNodeNumID != rootNodeNumID) ))
         continue;

      outContDirIDs->push_back(dirID);
      numEntries++;
   }

   if(!dirEntry && errno)
   {
      LogContext(logContext).logErr(std::string("Unable to fetch dentry from: ") +
         path + ". SysErr: " + System::getErrString() );
   }
   else
   { // all entries read
      retVal = FhgfsOpsErr_SUCCESS;
   }


   closedir(dirHandle);

return_res:
   return retVal;
}

/**
 * Note: intended to be used with fsck only
 */
bool StorageTkEx::getNextContDirID(unsigned hashDirNum, bool buddyMirrored, int64_t lastOffset,
   std::string* outID, int64_t* outNewOffset)
{
   *outID = "";
   StringList outIDs;
   FhgfsOpsErr retVal = getContDirIDsIncremental(hashDirNum, buddyMirrored, lastOffset, 1, &outIDs,
      outNewOffset);
   if ( ( outIDs.empty() ) ||  ( retVal != FhgfsOpsErr_SUCCESS ) )
      return false;
   else
   {
      *outID = outIDs.front();
      return true;
   }
}
