#include <common/Common.h>
#include <common/components/worker/DummyWork.h>
#include <common/nodes/LocalNode.h>
#include <common/storage/striping/Raid0Pattern.h>
#include <common/toolkit/NodesTk.h>
#include <common/toolkit/StorageTk.h>
#include <common/toolkit/MapTk.h>


#include <attr/xattr.h>
#include <app/App.h>
#include <program/Program.h>
#include <storage/DirEntry.h>

#include "Upgrade.h"

#include <fstream>

#define FHGFS_VERSION "2012.10"

#define ID_MAP_FILE_COLUMS (2)

#define SUFFIX_LENGTH (2) // -f or -d

#define ID_SEPARATOR ('-') // dash is used as seperator

#define STRING_NOT_FOUND_RET (-1)


/* general note: "old" as suffix means 2011.04 storage version, "new" as suffix means 2012.10
   storage version */


void Upgrade::printUsage(std::string progName)
{
   std::cout << std::endl;
   std::cout << "FhGFS Meta Server Format Upgrade Tool (2011.04 -> 2012.10)" << std::endl;
   std::cout << "Version: " << FHGFS_VERSION << std::endl;
   std::cout << "http://www.fhgfs.com" << std::endl;
   std::cout << std::endl;
   std::cout << "* Usage: " << std::endl;
   std::cout << "   " << progName << "               \\" << std::endl;
   std::cout << "        <storeMetaDirectory=storage_directory> \\" << std::endl;
   std::cout << "        <metaIdMapFile=metaIDMapFile>          \\" << std::endl;
   std::cout << "        <targetIdMapFile=targetIDMapFile>      \\" << std::endl;
   std::cout << "        (<upgradeDeleteOldFiles=true)" << std::endl;
   std::cout << std::endl;
   std::cout << "* Example: " << std::endl;
   std::cout << "# " << progName << "              \\" << std::endl;
   std::cout << "       storeMetaDirectory=/data/fhgfs_meta/  \\" << std::endl;
   std::cout << "       metaIdMapFile=/root/metaIDMapFile     \\" << std::endl;
   std::cout << "       targetIdMapFile=/root/targetIDMapFile" << std::endl;
   std::cout << std::endl;
   std::cout << "* Note: " << std::endl;
   std::cout << "    Consider a metadata backup before starting the upgrade process (e.g. to" << std::endl;
   std::cout << "    prevent problems in case of a power outage during the upgrade)." << std::endl;
   std::cout << "    " << std::endl;
   std::cout << std::endl;
   std::cout << "* Recommendation: " << std:: endl;
   std::cout << "    Run 'find /path/to/fhgfs_meta' before starting the upgrade process."<< std::endl;
   std::cout << "    This command will pre-cache important data to reduce disk seek time" << std::endl;
   std::cout << "    and thus, speed up the overall upgrade process." << std::endl;

   std::cout << std::endl;
}


bool Upgrade::updateEntryID(std::string inEntryID,  std::string& outEntryID)
{
   const char* logContext = "Update EntryID to numeric host id";
   App* app = Program::getApp();
   size_t firstSepPos;
   size_t secondSepPos;
   std::string stringID;
   uint16_t numID = 0;
   bool mapRes;

   // root (parentID then empty) and disposal are exceptions
   if (inEntryID == META_ROOTDIR_ID_STR || inEntryID == META_DISPOSALDIR_ID_STR ||
      inEntryID.empty() )
   {
      outEntryID = inEntryID;
      return true;
   }

   StringUnsignedMap* metaIdMap = app->getMetaStringIdMap();

   firstSepPos = inEntryID.find(ID_SEPARATOR);
   if (firstSepPos == (size_t) STRING_NOT_FOUND_RET)
      goto outerr;

   secondSepPos = inEntryID.find(ID_SEPARATOR, firstSepPos + 1);
   if (secondSepPos == (size_t) STRING_NOT_FOUND_RET)
      goto outerr;

   stringID = inEntryID.substr(secondSepPos + 1);

   mapRes = mapStringToNumID(metaIdMap, stringID, numID);
   if (!mapRes )
      goto outerr;

   outEntryID = inEntryID.substr(0, secondSepPos + 1) + StringTk::uint16ToHexStr(numID);

   return true;

outerr:
   LogContext(logContext).logErr("Failed to update entryID: " + inEntryID);
   outEntryID = inEntryID; // make mapping error non-fatal
   return false;
}

/**
 * Migrate a single dirEntry
 */
bool Upgrade::migrateDirEntry(std::string storageDir, std::string parentEntryID,
   DirInode* parentDirInode, std::string relDentriesPath, std::string entryName)
{
   const char* logContext = "migrate DirEntry";
   App* app = Program::getApp();
   Config* cfg = app->getConfig();
   bool deleteOldFiles = cfg->getDeleteOldFiles();
   MetaStore* metaStore = app->getMetaStore();

   bool retVal;
   FileInode* fileInode = NULL;
   EntryInfo entryInfo;

   std::string absDentriesPath = storageDir + "/" META_DENTRIES_SUBDIR_NAME_OLD "/" +
      relDentriesPath;
   std::string entryPathOld = absDentriesPath + "/" + entryName;
   std::string inodePath;
   std::string relFileInodePath;

   DirEntry* fhgfs_dirEntry = DirEntry::createFromFile(absDentriesPath, entryName);
   if (!fhgfs_dirEntry)
   {
      LogContext(logContext).logErr("Failed to create DirEntry from file: " + entryPathOld);
      return false;
   }

   bool dentryMapRes = fhgfs_dirEntry->mapOwnerNodeStrID(this->metaIdMap);
   if (dentryMapRes == false)
   {
      LogContext(logContext).logErr("OwnerNodeID mapping failed for: " + entryPathOld);
      retVal = false;
      goto out;
   }

   fhgfs_dirEntry->getEntryInfo(parentEntryID, 0, &entryInfo);

   if (DirEntryType_ISFILE(entryInfo.getEntryType() ) )
   {
      FhgfsOpsErr mkDentryRes;

      fileInode = FileInode::createFromEntryInfo(&entryInfo);
      if (!fileInode)
      {
         LogContext(logContext).logErr("");
         LogContext(logContext).logErr("Failed to deserialize inode for: " + entryPathOld);
         retVal = false;
         goto out;
      }

      bool mapRes = fileInode->mapStripePatternStringIDs(&this->targetIdMap);
      if (mapRes == false)
      {
         LogContext(logContext).logErr("");
         LogContext(logContext).logErr("targetID mapping failed for: " + entryPathOld);
         retVal = false;
         goto out;
      }

      inodePath = fileInode->getPathToInodeFile();
      relFileInodePath = fileInode->getReInodeFilePathPath();

      // deletes fileInode!
      mkDentryRes = metaStore->mkMetaFileUnlocked(parentDirInode, entryName,
         entryInfo.getEntryType(), fileInode);
      if (mkDentryRes != FhgfsOpsErr_SUCCESS && mkDentryRes != FhgfsOpsErr_EXISTS)
      {
         LogContext(logContext).logErr("");
         LogContext(logContext).logErr("Failed to create updated dentry for: " + entryPathOld);
         retVal =  false;
         goto out;
      }

      retVal = true;
   }
   else
   {
      FhgfsOpsErr mkDirRes = parentDirInode->makeDirEntry(fhgfs_dirEntry);
      fhgfs_dirEntry = NULL;
      if(mkDirRes != FhgfsOpsErr_SUCCESS && mkDirRes != FhgfsOpsErr_EXISTS)
      {
         LogContext(logContext).logErr("");
         LogContext(logContext).logErr("Failed to create updated dir-dentry for: " + entryPathOld);
         retVal = false;
         goto out;
      }

      retVal = true;
   }


   // delete or backup migrated dentries and their inodes
   if (retVal == true)
   {
      /* Always first the dentry, then the inode. If we would do it the other way around and would
       * need to re-run the upgrade tool, it will complain loudly if it cannot find an inode for
       * a dentry. */

      if (deleteOldFiles)
      {
         // first the dentry
         int unlinkDentryRes = unlink(entryPathOld.c_str() );
         if (unlinkDentryRes)
         {
            this->allOK = false;
            LogContext(logContext).logErr("");
            LogContext(logContext).logErr("Failed to unlink old dentry: " + entryPathOld);
         }
         else
         if (DirEntryType_ISFILE(entryInfo.getEntryType() ) )
         {
            // now the inode (for non-directories).
            int unlinkRes = unlink(inodePath.c_str() );
            if (unlinkRes)
            {
               LogContext(logContext).logErr("");
               LogContext(logContext).logErr("Failed to unlink old fileInode: " + inodePath + " :" +
                   System::getErrString() );
            }
         }
      }
      else
      {
         // first the dentry

         std::string backupPath = storageDir +
            "/" META_DENTRIES_SUBDIR_NAME_OLD META_UPGRADE_BACKUP_EXT "/" +
            relDentriesPath + "/" + entryName;
         int renameRes = rename(entryPathOld.c_str(), backupPath.c_str() );
         if (renameRes)
         {
            LogContext(logContext).logErr("");
            LogContext(logContext).logErr("Failed to rename: " + entryPathOld + " to: " +
               backupPath + " SysErr: " + System::getErrString() );
            this->allOK = false;
         }
         else
         if (DirEntryType_ISFILE(entryInfo.getEntryType() ) )
         {
            // now the inode

            std::string inodeBackupFilePath = this->inodesBackupPath + "/" + relFileInodePath;
            int renameRes = rename(inodePath.c_str(), inodeBackupFilePath.c_str() );
            if (renameRes)
            {
               LogContext(logContext).logErr("");
               LogContext(logContext).logErr("Failed to rename FileInode to backup dir: " +
                  inodePath + " :" + System::getErrString() );
            }
         }

      }

      /* No else here, as there is no reason to move dentries around, the 'structure' dir is the
       * backup.
       * Note: The only reason to move inodes is the migrateInodes step - it only shall migrate
       *       inodes not already handled by the dentry-migration */
   }

out:
   SAFE_DELETE(fhgfs_dirEntry);
   return retVal;
}

/**
 * Migrate a DirInode
 *
 * Note: We create this inode in the disposal dir, as there is no corresponding DirEntry for this
 *       inode.
 */
bool Upgrade::migrateFileInode(std::string inodeID)
{
   const char* logContext = "migrage FileInode";
   App* app = Program::getApp();
   bool deleteOldFiles = app->getConfig()->getDeleteOldFiles();
   MetaStore* metaStore = app->getMetaStore();
   bool retVal;

   EntryInfo entryInfo(0, "", inodeID, "", DirEntryType_REGULARFILE, 0); // just basic values

   FileInode* fileInode = FileInode::createFromEntryInfo(&entryInfo);
   if (!fileInode)
   {
      LogContext(logContext).logErr("");
      LogContext(logContext).logErr("Failed to deserialize FileInode: " + inodeID);
      return false;
   }

   bool mapRes = fileInode->mapStripePatternStringIDs(&this->targetIdMap);
   if (mapRes == false)
   {
      LogContext(logContext).logErr("");
      LogContext(logContext).logErr("StripePattern mapping failed for: " + inodeID);
      SAFE_DELETE(fileInode);
      return false;
   }

   std::string inodePath = fileInode->getPathToInodeFile();
   std::string hashDirInodePath = fileInode->getReInodeFilePathPath();

   FhgfsOpsErr mkDentryRes = metaStore->makeFileInode(fileInode, false);
   if (mkDentryRes != FhgfsOpsErr_SUCCESS && mkDentryRes != FhgfsOpsErr_EXISTS)
   {
      LogContext(logContext).logErr("");
      LogContext(logContext).logErr("Failed to create updated inode for fileInode ID: " + inodeID);
      retVal =  false;
      goto out;
   }

   retVal = true;

   if (deleteOldFiles)
   {
      int unlinkRes = unlink(inodePath.c_str() );
      if (unlinkRes)
      {
         LogContext(logContext).logErr("Failed to unlink old fileInode: " + inodePath + " :"
            + System::getErrString() );
      }
   }
   else
   { // move to backup dir
      std::string inodeBackupDir = this->inodesBackupPath + "/" + hashDirInodePath;
      int renameRes = rename(inodePath.c_str(), inodeBackupDir.c_str() );
      if (renameRes)
         LogContext(logContext).logErr("Failed to rename FileInode to backup dir: " +
            inodePath + " :" + System::getErrString() );
   }

out:
   return retVal;
}


/**
 * Delete the inode on disk or move it into the backup dir.
 *
 */
bool Upgrade::backupOrDeleteDirInode(DirInode* inode)
{
   App* app = Program::getApp();
   bool deleteOldEntries = app->getConfig()->getDeleteOldFiles();
   bool retVal = true;

   if (deleteOldEntries)
   {
      const char* logContext = "Delete DirInode";

      int unlinkRes = unlink(inode->getPathToDirInode() );
      if (unlinkRes)
      {
         retVal = false;
         this->allOK = false;
         LogContext(logContext).logErr("");
         LogContext(logContext).logErr(std::string("Failed to delete old DirInode: ") +
            inode->getPathToDirInode() + std::string(" :") + System::getErrString() );
      }
   }
   else
   { // move to backup dir
      const char* logContext = "Backup DirInode";

      std::string inodeBackupDir = this->inodesBackupPath + inode->getOldHashDirPath();
      int renameRes = rename(inode->getPathToDirInode(), inodeBackupDir.c_str() );
      if (renameRes && errno != EEXIST)
      {
         retVal = false;
         this->allOK = false;
         LogContext(logContext).logErr("");
         LogContext(logContext).logErr(std::string("Failed to rename DirInode to backup dir: ") +
            inode->getPathToDirInode()  + std::string(": ") + System::getErrString() );
      }
   }

   return retVal;
}

/**
 * Migrate a DirInode
 */
bool Upgrade::migrateDirInode(std::string inodeID, DirInode** outInode)
{
   const char* logContext = "Migrate DirInode";
   App* app = Program::getApp();
   MetaStore* metaStore = app->getMetaStore();

   *outInode = NULL;

   DirInode* inode = DirInode::createFromFile(inodeID);
   if (!inode)
   {
      LogContext(logContext).logErr("");
      LogContext(logContext).logErr("Failed to deserialize DirInode: " + inodeID );
      this->allOK = false;
      return false;
   }

   bool mapRes = inode->mapStringIDs(&this->targetIdMap, this->metaIdMap);
   if (mapRes == false)
   {
      LogContext(logContext).logErr("");
      LogContext(logContext).logErr("Dir-ID mapping failed for: " + inodeID);
      SAFE_DELETE(inode);
      this->allOK = false;
      return false;
   }

   FhgfsOpsErr makeDirInodeRes = metaStore->makeDirInode(inode, true);
   if (makeDirInodeRes != FhgfsOpsErr_SUCCESS && makeDirInodeRes != FhgfsOpsErr_EXISTS)
   {
      LogContext(logContext).logErr("");
      LogContext(logContext).logErr("Failed to create migrated dirInode: " + inodeID);
      SAFE_DELETE(inode);
      this->allOK = false;
      return false;
   }

   *outInode = inode;
   return true;
}

/**
 * Migrate dentries of the given dir
 *
 * @path path to dentries subdir (where we will read the dentry files).
 * @newTmpPath path to new temporary structure subdir (where we will put the new link files);
 * will be created.
 */
bool Upgrade::migrateDentryDir(std::string storageDir, std::string parentEntryID,
   std::string relDentriesPath)
{
   const char* logContext = "Migrate DentriesDir (structure)";
   App* app = Program::getApp();
   bool deleteOldFiles = app->getConfig()->getDeleteOldFiles();

   if (!deleteOldFiles)
   {
      std::string backupPath = storageDir +
         "/" META_DENTRIES_SUBDIR_NAME_OLD META_UPGRADE_BACKUP_EXT "/" + relDentriesPath;
      if (!doMkDir(backupPath) )
         return false;
   }

   // walk the old .cont dir and migrate each contained file
   unsigned migrateErrors = 0;

   int parentLen = parentEntryID.length();

   if (parentLen < DENTRYDIR_CONT_LEN + 1)
   {
      LogContext(logContext).logErr("Invalid parentID (too small: " + parentEntryID + ")" );
      this->allOK = false;
      return false;
   }

   std::string contSubStr = parentEntryID.substr(parentLen - DENTRYDIR_CONT_LEN, DENTRYDIR_CONT_LEN);

   if (contSubStr != DENTRYDIR_CONT)
   {
      LogContext(logContext).logErr(std::string("Invalid parentEntryID (.cont missing): ") +
         contSubStr);
      this->allOK = false;
      return false;
   }

   // remove .cont from parentEntryID
   std::string parentIDWithoutCont(parentEntryID);
   parentIDWithoutCont.resize(parentLen - DENTRYDIR_CONT_LEN);

   DirInode* parentDirInode;
   bool dirInodeRes = migrateDirInode(parentIDWithoutCont, &parentDirInode);
   if (!dirInodeRes)
   {
      this->allOK = false;
      return false;
   }

   std::string absDentriesPath = storageDir + "/" META_DENTRIES_SUBDIR_NAME_OLD "/" +
      relDentriesPath;
   DIR* dirHandle = opendir(absDentriesPath.c_str() );
   if(!dirHandle)
   {
      LogContext(logContext).logErr("Unable to open structure subdir: " + absDentriesPath + ". " +
         "SysErr: " + System::getErrString() );
      this->allOK = false;
      return false;
   }


   // uint64_t numEntries = 0;
   struct dirent* dirEntry = NULL;

   errno = 0; // recommended by posix (readdir(3p) )

   // read all entries and move them to new tmp dir
   do {
      dirEntry = StorageTk::readdirFiltered(dirHandle);

      if (!dirEntry)
         break;

      std::string entryName = dirEntry->d_name;

      bool migrateRes = migrateDirEntry(storageDir, parentIDWithoutCont, parentDirInode,
         relDentriesPath, entryName);
      if (migrateRes == false)
         migrateErrors++;

   } while(dirEntry);

   bool retVal;

   if (!migrateErrors)
   {
      retVal = true;
      backupOrDeleteDirInode(parentDirInode);
   }
   else
   {
      retVal = false;
      this->allOK = false;
   }

   SAFE_DELETE(parentDirInode);
   closedir(dirHandle);

   if (retVal == true)
   {
      int rmDirRes = rmdir(absDentriesPath.c_str() );
      if (rmDirRes)
      {
         LogContext(logContext).logErr("");
         LogContext(logContext).logErr("Failed to rmdir the contents dir: " + absDentriesPath +
            ": " + System::getErrString() );
         this->allOK = false;
         retVal = false;
      }
   }

   return retVal;
}

/**
 * Converts the directory entry (dentry) files in the dentries-subdir to the new format and stores
 * into the correct dir. Also inlines inodes.
 */
bool Upgrade::migrateDentries(std::string storageDir)
{
   const char* logContext = "migrate dentries";
   App* app = Program::getApp();
   bool deleteOldFiles = app->getConfig()->getDeleteOldFiles();

   uint64_t numUserDirs = 0;
   struct dirent* dirEntry = NULL;

   std::string oldDentriesPath = storageDir + "/" META_DENTRIES_SUBDIR_NAME_OLD;

   if (!deleteOldFiles)
   {
      std::string backupDir = oldDentriesPath + META_UPGRADE_BACKUP_EXT;
      if (!doMkDir(backupDir) )
         return false;
   }

   LogContext(logContext).logErr("* Migrating dentries... (Processing " +
      StringTk::uint64ToStr(HASHDIRS_NUM_OLD) + " hash directories...)" );
   LogContext(logContext).log(Log_NOTICE, "Progress: ");

   bool retVal = true;

   // walk over all hash subdirs ('dentries', formerly called 'structure')
   for(unsigned i=0; i < HASHDIRS_NUM_OLD; i++)
   {
      std::string subDirHash = StringTk::uintToHexStr(i);
      std::string subdirPath = oldDentriesPath + "/" + subDirHash;

      LogContext(logContext).log(Log_NOTICE, subDirHash + " ", true);

      if (!deleteOldFiles)
      {
         std::string backupPath = storageDir +
            "/" META_DENTRIES_SUBDIR_NAME_OLD META_UPGRADE_BACKUP_EXT "/" + subDirHash;
         if (!doMkDir(backupPath) )
            continue;
      }


      DIR* dirHandle = opendir(subdirPath.c_str() );
      if(!dirHandle && errno == ENOENT)
         continue;
      else
      if(!dirHandle)
      {
         LogContext(logContext).logErr("Unable to open hash directory: " + subdirPath + ". " +
            "SysErr: " + System::getErrString() );

         goto err_cleanup;
      }

      errno = 0;

      // walk over all parentEntryIDs
      while( (dirEntry = StorageTk::readdirFiltered(dirHandle) ) )
      {
         std::string parentEntryID = dirEntry->d_name; // has the .cont suffix
         std::string relDentriesPath = subDirHash + "/" + parentEntryID;

         if (dirEntry->d_type != DT_DIR && dirEntry->d_type != DT_UNKNOWN)
            this->hasInvalidDirectoryType = true;

         bool migrateRes = migrateDentryDir(storageDir, parentEntryID, relDentriesPath);

         if(!migrateRes)
         {
            std::string absPath = storageDir + "/" META_DENTRIES_SUBDIR_NAME_OLD "/" +
               relDentriesPath;
            LogContext(logContext).logErr("Unable to convert directory: " + absPath + ".");
            retVal = false;
            this->allOK = false;
            continue;
         }

         numUserDirs++;
      }

      if(!dirEntry && errno)
      {
         LogContext(logContext).logErr("");
         LogContext(logContext).logErr("Unable to fetch directory entry from: " + oldDentriesPath +
            ". " + "SysErr: " + System::getErrString() );

         goto err_closedir;
      }

      closedir(dirHandle);

      // everything was ok, delete the hash dir
      {
         int rmDirRes = rmdir(subdirPath.c_str() );
         if (rmDirRes)
         {
            LogContext(logContext).logErr("");
            LogContext(logContext).logErr("Failed to rmdir the dentries hash dir: " + subdirPath +
               ": " + System::getErrString() );
         }
      }

      continue;

      err_closedir:
         closedir(dirHandle);
         retVal = false;
         this->allOK = false;
         continue;

   }

   LogContext(logContext).logErr(""); // just an EOL
   LogContext(logContext).logErr("* Converted user directories: " +
      StringTk::uint64ToStr(numUserDirs) );

   if (retVal == true && this->allOK == true)
   {
      int rmDirRes = rmdir(oldDentriesPath.c_str() );
      if (rmDirRes)
      {
         LogContext(logContext).logErr("");
         LogContext(logContext).logErr("Failed to rmdir the old structure dir: " + oldDentriesPath +
            " :" + System::getErrString() );
      }
   }


   return retVal;


err_cleanup:

   return false;
}

bool Upgrade::migrateInode(std::string inodeID, std::string inodePath, std::string subDirHash)
{
   const char* logContext = "migrage Inode";

   bool isDirInode;

   unsigned strLen = inodeID.length();
   std::string suffix = inodeID.substr(strLen - 2, SUFFIX_LENGTH);

   if (suffix.compare("-d") == 0 )
      isDirInode = true;
   else
   if (suffix.compare("-f") == 0)
      isDirInode = false;
   else
   {
      LogContext(logContext).logErr("");
      LogContext(logContext).logErr("Inode is neither a dir nor a directory: " + inodePath );
      this->allOK = false;
      return false;
   }

   std::string idWithOutSuffix = inodeID;
   idWithOutSuffix.resize(strLen - 2);
   bool migrateRes;

   if (isDirInode)
   {
      // Hmm, shall we really do anything here? Actually migrate migrateDentries() is a better
      // place to handle DirInodes. If there is no contents dir, this DirInode can be deleted,
      // as there are no entries...

      DirInode* inode = NULL;

      migrateRes = migrateDirInode(idWithOutSuffix, &inode);
      if (migrateRes == true)
         backupOrDeleteDirInode(inode);

      SAFE_DELETE(inode);

      return migrateRes;
   }
   else
      migrateRes = migrateFileInode(idWithOutSuffix);

   return migrateRes;
}

/**
 * Converts all inodes in the inodes-dir (fomerly 'entries') to the new format and stores
 * into the correct dir.
 */
bool Upgrade::migrateInodes(std::string storageDir)
{
   const char* logContext = "Migrate Inodes";
   uint64_t numInodes = 0;
   struct dirent* inodeEntry = NULL;
   bool retVal = true;

   std::string oldInodesPath = storageDir + "/" META_INODES_SUBDIR_NAME_OLD;

   LogContext(logContext).logErr("* Migrating inodes... (Processing " +
      StringTk::uint64ToStr(HASHDIRS_NUM_OLD) + " hash directories...)" );
   LogContext(logContext).log(Log_NOTICE, "* Progress: ");

   // walk over all hash subdirs
   for(unsigned i=0; i < HASHDIRS_NUM_OLD; i++)
   {
      std::string subDirHash = StringTk::uintToHexStr(i);
      std::string subdirPath = oldInodesPath + "/" + subDirHash;
      bool migrateErrors = false;

      // print dir num as progress info (without newline)
      LogContext(logContext).log(Log_NOTICE, subDirHash + " ", true);

      DIR* dirHandle = opendir(subdirPath.c_str() );
      if(!dirHandle && errno == ENOENT)
         continue;
      else
      if(!dirHandle)
      {
         LogContext(logContext).logErr("Unable to open hash directory: " + subdirPath + ". " +
            "SysErr: " + System::getErrString() );

         goto err_cleanup;
      }

      errno = 0;

      // walk over all inodes of the hash dir
      while( (inodeEntry = StorageTk::readdirFiltered(dirHandle) ) )
      {
         if (inodeEntry->d_type != DT_DIR && inodeEntry->d_type != DT_UNKNOWN)
            this->hasInvalidDirectoryType = true;

         std::string inodeID = inodeEntry->d_name; // has the -d or -f suffix
         std::string inodePath = subdirPath + "/" + inodeID;

         bool migrateRes = migrateInode(inodeID, inodePath, subDirHash);
         if(!migrateRes)
         {
            migrateErrors = true;
            retVal = false;
            LogContext(logContext).logErr("Unable to convert inode: " + inodePath + ".");
            errno = 0;
            continue;
         }

         numInodes++;
         errno = 0;
      }

      if(!inodeEntry && errno)
      {
         LogContext(logContext).logErr("Unable to fetch directory entry from: " + oldInodesPath + ". " +
            "SysErr: " + System::getErrString() );
      }

      closedir(dirHandle);

      if (!migrateErrors)
      {
         int rmDirRes = rmdir(subdirPath.c_str() );
         if (rmDirRes)
            LogContext(logContext).logErr("Failed to rmdir inodes (entries) hash dir: " + subdirPath +
               " :" + System::getErrString() );
      }

   }

   LogContext(logContext).logErr(""); // just an EOL
   LogContext(logContext).logErr("* Converted additional inodes: " +
      StringTk::uint64ToStr(numInodes) );

   if (retVal == true)
   {
      int rmDirRes = rmdir(oldInodesPath.c_str() );
      if (rmDirRes && errno != ENOENT)
         LogContext(logContext).logErr("Failed to rmdir the entries dir: " + oldInodesPath + " :" +
            System::getErrString() );
   }
   else
      this->allOK = false;

   return retVal;


err_cleanup:

   return false;
}


void Upgrade::printFinalInfo()
{
   const char* logContext = "Final Info";
   LogContext log(logContext);

   LogContext(logContext).logErr(""); // just an EOL
   if (this->allOK == true)
      log.logErr("* Format conversion complete without any problems.");
   else
   {
      log.logErr("* Format conversion complete with errors!");
      log.logErr("  You probably want to fix those and re-run the upgrade tool!");
   }

   if (this->hasInvalidDirectoryType == true)
   {
      log.logErr("");
      log.logErr("* Some of the hash directories do not have the correct Direntry-Type.");
      log.logErr("  Please consider to fsck the underlying block device!");
   }
}

bool Upgrade::loadIDMap(std::string mapFileName, StringUnsignedMap* outIdMap)
{
   const char* logContext = "Load IP map file";

   std::ifstream fis(mapFileName.c_str() );
   if(!fis.is_open() || fis.fail() )
   {
      LogContext(logContext).logErr("Failed to open ID-map-file: " + System::getErrString() );

      return false;
   }

   bool retVal = true;
   char line[STORAGETK_FILE_MAX_LINE_LENGTH];
   while(!fis.eof() && !fis.fail() )
   {
      fis.getline(line, STORAGETK_FILE_MAX_LINE_LENGTH);
      std::string trimmedLine = StringTk::trim(line);

      if(trimmedLine.length() && (trimmedLine[0] != STORAGETK_FILE_COMMENT_CHAR) )
      {
         unsigned numID;
         char stringID[513];

         int scanRes = sscanf(trimmedLine.c_str(), "%48s %u", stringID, &numID);
         if (scanRes == ID_MAP_FILE_COLUMS)
            outIdMap->insert(StringUnsignedMapVal(stringID, numID) );
         else
         {
            LogContext(logContext).logErr("Invalid line in map file (" + mapFileName + ")" +
               std::string(" Line: ") + line);
            break;
            retVal = false;
         }
      }
   }

   fis.close();

   return retVal;
}

bool Upgrade::setLocalNodeNumID(StringUnsignedMap* metaIdMap)
{
   App* app = Program::getApp();

   std::string stringID = app->getLocalNodeStrID();
   uint16_t numID = 0;

   bool mapRes = mapStringToNumID(metaIdMap, stringID, numID);
   if (!mapRes)
      return false;

   app->setLocalNodeNumID(numID);
   app->initLocalNodeNumIDFile();

   return true;
}


bool Upgrade::doMkDir(std::string path)
{
   const char* logContext = "mkdir";
   int mkDirRes = mkdir(path.c_str(), S_IRWXU | S_IRWXG | S_IRWXO );
   if (mkDirRes)
   {
      if (errno == EEXIST)
      {
         // path exists already => check whether it is a directory
         struct stat statStruct;
         int statRes = stat(path.c_str(), &statStruct);
         if(statRes || !S_ISDIR(statStruct.st_mode) )
         {
            LogContext(logContext).logErr("mkdir path already exists, but is not a directory: " +
               path);
            return false;
         }

         return true; // already exists
      }

      LogContext(logContext).logErr("Failed to create directory: " + path + ": " +
         System::getErrString() );
      return false;
   }

   return true;
}

/**
 * Converts all inodes in the inodes-dir (fomerly 'entries') to the new format and stores
 * into the correct dir.
 */
bool Upgrade::initInodesBackupDir(std::string storageDir)
{
   const char* logContext = "initialize the entries backup dir";
   this->inodesBackupPath = storageDir + "/" META_INODES_SUBDIR_NAME_OLD META_UPGRADE_BACKUP_EXT;

   if (!doMkDir(inodesBackupPath) )
   {
      LogContext(logContext).logErr("Failed to create inodes backup dir. Aborting!");
      return false;
   }

   // create  inode backup hash subdirs
   for(unsigned i=0; i < HASHDIRS_NUM_OLD; i++)
   {
      std::string subDirHash = StringTk::uintToHexStr(i);
      std::string subdirPath = inodesBackupPath + "/" + subDirHash;

      if (!doMkDir(subdirPath) )
      {
         LogContext(logContext).logErr("Failed to create inodes hash backup dir. Aborting!");
         return false;
      }
   }

   return true;
}


int Upgrade::runUpgrade()
{
   App* app = Program::getApp();
   Config* config = app->getConfig();
   std::string storageDir = config->getStoreMetaDirectory();

   this->metaIdMap = Program::getApp()->getMetaStringIdMap();

   LogContext log("");
   log.logErr(""); // just an EOL
   log.logErr("* Writing log file to: " + config->getLogStdFile() );
   log.logErr("  Progress may be monitored by watching this file.");


   // loads target string to numeric ID map
   std::string targetIDMapFileName = Program::getApp()->getConfig()->getTargetIdMapFile();
   if (!loadIDMap(targetIDMapFileName, &this->targetIdMap) )
      return 1;

   bool deleteOldFiles = config->getDeleteOldFiles();
   if (!deleteOldFiles)
   {
      bool initRes = initInodesBackupDir(storageDir);
      if (!initRes)
         return 1;
   }

   if (!setLocalNodeNumID(this->metaIdMap) )
      return 1;

   log.logErr(""); // just an EOL
   log.logErr("* Starting the upgrade process: ...");
   log.logErr(""); // just an EOL

   bool retVal = migrateDentries(storageDir); // walks and calls migrateStructureSubdir()
   if(retVal == false || this->allOK == false)
   {
      log.logErr("");
      log.logErr("Not all dentries could be migrated. Remaining inodes will be ignored!");
      retVal = false;
      goto out;
   }

   if (!migrateInodes(storageDir) ) // migrate all (remaining) inodes
      retVal = false;


out:
   // storage format file
   if(!StorageTkEx::createStorageFormatFile(storageDir) )
      throw InvalidConfigException("Unable to create storage format file in: " + storageDir);

   printFinalInfo();

   return retVal;
}
