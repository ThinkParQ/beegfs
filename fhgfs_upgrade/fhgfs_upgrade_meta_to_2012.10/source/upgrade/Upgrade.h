#ifndef UPGRADE_H
#define UPGRADE_H

#include <common/Common.h>

class DirInode; // forward declaration

#ifndef FHGFS_VERSION
   #define FHGFS_VERSION   "2012.10"
#endif

#define LOGFILE_PATH         "/var/tmp/fhgfs-upgrade-meta." // suffixed with PID
#define LOGFILE_PATH_SUFFIX  ".log"

#define HASHDIRS_NUM_OLD              (8192)

#define META_INODES_LEVEL1_SUBDIR_NUM_NEW (128)
#define META_INODES_LEVEL2_SUBDIR_NUM_NEW (128)

#define META_UPGRADE_BACKUP_EXT          ".bak"

#define META_INODES_SUBDIR_NAME_OLD     "entries"  /* contains local file system entry metadata */
#define META_INODES_SUBDIR_NAME_NEW     "inodes"   /* contains local file system entry metadata */

#define META_DENTRIES_SUBDIR_NAME_OLD   "structure"  /* contains file system link structure */
#define META_DENTRIES_SUBDIR_NAME_NEW   "dentries"   /* contains file system link structure */

#define DENTRYDIR_CONT ".cont"
#define DENTRYDIR_CONT_LEN (5)

#define META_DENTRIES_LEVEL1_SUBDIR_NUM (128)
#define META_DENTRIES_LEVEL2_SUBDIR_NUM (128)

#define NODEID_FILENAME               "nodeID"
#define TARGETID_FILENAME             "targetID"

#define META_SUBFILE_SUFFIX_STR_OLD   "-f"
#define META_SUBDIR_SUFFIX_STR_OLD    "-d"

#define META_XATTR_DENTRY_NAME_OLD     "user.fhgfs_link" /* attribute name for dir-entries */
#define META_XATTR_FILEINODE_NAME_OLD  "user.fhgfs_file" /* attribute name for dir metadata */
#define META_XATTR_DIRINODE_NAME_OLD   "user.fhgfs_dir"  /* attribute name for file metadata */

#define META_XATTR_LINK_NAME_NEW       "user.fhgfs" /* attribute name for dir-entries */
#define META_XATTR_FILE_NAME_NEW       "user.fhgfs" /* attribute name for dir metadata */
#define META_XATTR_DIR_NAME_NEW        "user.fhgfs" /* attribute name for file metadata */

#define FILE_INODE_FORMAT_VERSION_OLD    2
#define FILE_INODE_FORMAT_VERSION_NEW    3

#define DIR_INODE_FORMAT_VERSION_OLD    2
#define DIR_INODE_FORMAT_VERSION_NEW    3

// format.conf
#define META_VERSION_OLD     2
#define META_VERSION_NEW     3


class Upgrade
{
   public:

      Upgrade()
      {
         this->allOK = true;
         this->hasInvalidDirectoryType = false;
      }

      int runUpgrade();
      static bool loadIDMap(std::string mapFileName, StringUnsignedMap* idMap);
      static bool updateEntryID(std::string inEntryID, std::string& outEntryID);
      static void printUsage(std::string progName);

   private:
      StringUnsignedMap targetIdMap;
      StringUnsignedMap* metaIdMap;

      std::string inodesBackupPath;

      /* somehow invalid directory types are common and we don't want to print a log message for
       * all wrong dirs, but instead we are going to recommend a single time to fsck the device */
      bool hasInvalidDirectoryType;

      void printFinalInfo();

      bool migrateDentryDir(std::string storageDir, std::string parentEntryID,
         std::string dirPathOld);
      bool migrateDentries(std::string storageDir);
      bool createFormatFile(std::string storageDir, bool useXAttr);

      bool setLocalNodeNumID(StringUnsignedMap* metaIdMap);

      bool migrateDirEntry(std::string storageDir, std::string parentEntryID,
         DirInode* parentDirInode, std::string dirPathOld, std::string entryName);

      bool migrateFileInode(std::string inodeID);
      bool backupOrDeleteDirInode(DirInode* inode);
      bool migrateDirInode(std::string inodeID, DirInode** outInode);

      bool migrateInodes(std::string storageDir);
      bool migrateInode(std::string inodeID, std::string inodePath, std::string subDirHash);

      bool initInodesBackupDir(std::string storageDir);

      bool doMkDir(std::string path);

      bool allOK;


   public:

      static bool mapStringToNumID(StringUnsignedMap* idMap, std::string inStrID, uint16_t& outNumID)
      {
         StringUnsignedMapIter mapIter = idMap->find(inStrID);
         if (unlikely(mapIter == idMap->end() ) )
            return false; // not in map

         outNumID = mapIter->second;
         return true;

      }


};


#endif /* UPGRADE_H */
