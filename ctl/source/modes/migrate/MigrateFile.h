/*
 * File migration class
 */

#ifndef MIGRATEFILE_H_
#define MIGRATEFILE_H_

#include <common/toolkit/MetadataTk.h>
#include <common/storage/EntryInfo.h>
#include <toolkit/XAttrTk.h>

#include <dirent.h> // DT_LNK and DT_REG
#include "boost/optional.hpp"

#define BEEGFS_TMP_FILE_NAME  ".fhgfs_tmp_migrate_file." // thread ID MUST be added!

struct MigrateDirInfo
{
      MigrateDirInfo(std::string dirPath, int dirFD, EntryInfo* entryInfo)
      {
         this->dirPath = dirPath;
         this->dirFD = dirFD;
         this->entryInfo = entryInfo;
      }

      std::string dirPath; // path of the parent directory, used for error messages only

      // fd of the parent directory NOTE: only used for passing data to the object, will be dup()ed
      int dirFD;
      EntryInfo* entryInfo;
};

struct MigrateFileInfo
{
      MigrateFileInfo(std::string fileName, unsigned fileType, int numTargets,
         StoragePoolId storagePoolId,  UInt16Vector* destStorageTargetIDs)
      {
         this->fileName = fileName;
         this->fileType = fileType;
         this->numTargets = numTargets;
         this->storagePoolId = storagePoolId;
         this->destStorageTargetIDs = destStorageTargetIDs;
      }

      std::string fileName; // directory entry name, NOTE: NOT the complete path!
      unsigned fileType; // d_type of struct dirent, NOTE: FhGFS supports this
      unsigned numTargets;
      StoragePoolId storagePoolId;
      UInt16Vector* destStorageTargetIDs;
};

/* File object to copied/migrated. Except of the target list, we create it as its own object
 * pointers to other memory objects. That way we can later on paralyze the code and pass the
 * object (class) to a thread pool which handles all file copies.
 */
class MigrateFile
{
   public:
      MigrateFile(struct MigrateDirInfo* dirInfo, struct MigrateFileInfo* fileInfo)
      {
         this->dirPath = dirInfo->dirPath;

         this->dirFD = dup(dirInfo->dirFD);
         if (this->dirFD < 0)
            this->fdCloneError = strerror(errno);

         this->fileName = fileInfo->fileName;
         this->fileType = fileInfo->fileType;

         /* will be initialized before the ioctl and including the executing tid of the
          *  executing thread */
         this->tmpName = "";

         this->desiredNumTargets = fileInfo->numTargets;

         this->parentInfo.set(dirInfo->entryInfo);

         this->destStorageTargetIDs = fileInfo->destStorageTargetIDs;
         this->numDestTargets = this->destStorageTargetIDs->size();
         this->storagePoolId = fileInfo->storagePoolId;

         // limit the number of destination targets to really available targets
         if (this->desiredNumTargets > this->numDestTargets)
            this->desiredNumTargets = this->numDestTargets;

         if (this->fileType == DT_LNK)
            this->desiredNumTargets = 1; // links really only need one target

         this->filePath = this->dirPath + "/" + this->fileName;

      }

      ~MigrateFile()
      {
         close(this->dirFD);
      }

      bool doMigrate(void);

   private:
      std::string dirPath;   // path to the file
      std::string fileName;  // file name - actually a directory entry name, so without the path
      unsigned fileType;     // dentry->d_type, so the file type

      int dirFD; // file descriptor of the directory

      EntryInfo parentInfo;

      std::string tmpName;  // tmp file we copy the data into

      unsigned desiredNumTargets;
      UInt16Vector* destStorageTargetIDs; // not owned by this object!
      StoragePoolId storagePoolId;

      // globally valid for all file objects, number of available dest targets
      unsigned numDestTargets;

      std::string fdCloneError; // only set if cloning the fd failed

      /* entire path to the file including the fileName, just to simplify path print
       * for error messages */
      std::string filePath;

      // we create a global random generator, so that we really get a different number on each call
      static RandomReentrant randomGen;

      bool migrateRegularFile(void);
      bool migrateSymLink(void);
      bool getFromFileMeta(int *outFD, struct stat* outStatData);
      void removeTmpOnErr(void);
      int runCreateIoctl(struct stat* fromStat, const char *symlinkTo = NULL);
      bool createPreferredTargetsArray(uint16_t** outPrefTargets, int* outPrefTargetsLen);
      bool copyData(int fromFD, int tmpFD, struct stat* fromStatData);
      bool copyTimes(int fd, struct stat* fromStatData);
      bool copyTimesLink(struct stat* fromStatData);
      bool copyOwnerAndMode(int tmpFD, struct stat* fromStatData);
      bool fileWasModified(struct stat* fromStatData);
      bool copyOwnerAndModeLink(struct stat* fromStatData);

      boost::optional<XAttrMap> getXAttrs() const;
      bool copyXAttrs(const XAttrMap& fromXAttrs);
};

#endif /* MIGRATEFILE_H_ */
