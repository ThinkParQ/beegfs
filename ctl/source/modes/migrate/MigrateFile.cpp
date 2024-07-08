/*
 * Copy a file object, so that it gets other meta/storage targets.
 * So far, we only have storage target support.
 */

#include <toolkit/IoctlTk.h>
#include <toolkit/XAttrTk.h>
#include "MigrateFile.h"
#include "ModeMigrateException.h"

#include <utility>
#include <cstring>   // memset
#include <features.h> // _POSIX_C_SOURCE

#define BUFFER_SIZE (32 * 1024 * 1024)
#define MAX_LINK_SIZE 4096
#define MIGRATEFILE_NUM_PREFERRED_TARGETS (unsigned) 20


RandomReentrant MigrateFile::randomGen;

/**
* Retrieves a map that associates file types as defined in dirent.h with textual
* representations of these types. This should only be used to "pretty print" the 
* types for logging, and the authoritative source is the enum in dirent.h.
*/
const std::map<int, std::string>& getFileTypeMap()
{
    static const std::map<int, std::string> fileTypeMap = {
        {DT_UNKNOWN, "DT_UNKNOWN"},
        {DT_FIFO,    "DT_FIFO"},
        {DT_CHR,     "DT_CHR"},
        {DT_DIR,     "DT_DIR"},
        {DT_BLK,     "DT_BLK"},
        {DT_REG,     "DT_REG"},
        {DT_LNK,     "DT_LNK"},
        {DT_SOCK,    "DT_SOCK"},
        {DT_WHT,     "DT_WHT"}
    };

    return fileTypeMap;
}

/**
 * Converts a file type integer to its string representation. Used to pretty
 * print types in errors, for example if we are provided an unsupported type.
*/
std::string convertFileTypeToString(int type)
{
    const auto& map = getFileTypeMap();
    auto it = map.find(type);
    if (it != map.end()) {
        return it->second;
    } else {
        return "Unknown file type (" + std::to_string(type) + ") ";
    }
}

/**
 * Start the migration of the MigrateFile object here.
 */
bool MigrateFile::doMigrate()
{
   if (this->dirFD < 0)
   {
      std::cerr << "Invalid parent file descriptor: " << this->fdCloneError << std::endl;
      return false;
   }

   switch (this->fileType)
   {
      case DT_REG:
         return migrateRegularFile();
      case DT_LNK:
         // migrate symlink
         return migrateSymLink();
      default:
         std::cerr << "Unable to migrate file type (manually migrate if necessary): " << convertFileTypeToString(this->fileType) << std::endl;
         return false; // we no longer silently ignore unknown file types
   }

   return false;
}

/**
 * Migrate a regular file (DT_REG)
 */
bool MigrateFile::migrateRegularFile()
{
   // FD and stat data of the file we want to copy
   int fromFD;
   struct stat fromStatData;

   bool fromMeta = getFromFileMeta(&fromFD, &fromStatData);
   if (!fromMeta)
   {
      std::cerr << "Failed to get origin meta data" << std::endl;
      return false;
   }

   if (fromStatData.st_nlink > 1)
   {
      std::cerr << "Cannot migrate file with hard link (yet): " << this->filePath << std::endl;
      return false;
   }

   auto maybeFromXAttrs(getXAttrs());
   if (!maybeFromXAttrs)
   {
      return false;
   }
   XAttrMap fromXAttrs (std::move(*maybeFromXAttrs));

   int tmpFD = runCreateIoctl(&fromStatData);
   if (tmpFD < 0)
      return false; // failed to create new file

   bool metaRes;
   int renameRes;
   bool retVal = false;
   bool fileWasModified;

   metaRes = copyData(fromFD, tmpFD, &fromStatData); // copy file data (content)
   if (!metaRes)
   {
      removeTmpOnErr();
      goto out;
   }

   // only request a mode update if we had to remove some bits when we created the file
   if (fromStatData.st_mode & (S_ISVTX | S_ISUID | S_ISGID) )
   {
      metaRes = copyOwnerAndMode(tmpFD, &fromStatData); // mode and owner
      if (!metaRes)
      {
         removeTmpOnErr();
         goto out;
      }
   }

   /* Just before the rename and to keep a possible race window a short as possible, we do a
    * sanity check, if the file was changed in the mean time */
   fileWasModified = this->fileWasModified(&fromStatData);
   if (fileWasModified)
   {
      removeTmpOnErr();
      goto out;
   }

   // now rename the tmpFile, that will also delete the old file
   renameRes = renameat(this->dirFD, this->tmpName.c_str(), this->dirFD, this->fileName.c_str());
   if (renameRes)
   {
      std::cerr << "Renaming tmpFile failed (" << this->tmpName << "->" << this->filePath << "): "
         << System::getErrString() << std::endl;
      removeTmpOnErr();
      goto out;
   }

   // now copy over the time stamps, NOTE: although we renamed the file, tmpFD is still valid.
   metaRes = copyTimes(tmpFD, &fromStatData);
   if (!metaRes)
   {
      removeTmpOnErr();
      goto out;
   }

   // Copy extended attributes
   if (!copyXAttrs(fromXAttrs))
   {
      removeTmpOnErr();
      goto out;
   }

   retVal = true;

out:
   close(fromFD);
   close(tmpFD);

   return retVal;
}

/**
 * Migrate symbolic links
 */
bool MigrateFile::migrateSymLink()
{
   bool retVal = false;
   int res;
   int renameRes;
   int metaRes;
   struct stat fromStatData;
   bool wasModified;

   int statRes = fstatat(this->dirFD, this->fileName.c_str(), &fromStatData, AT_SYMLINK_NOFOLLOW);
   if (statRes < 0)
   {
      std::cerr << "Failed to stat (" << this->filePath << "): " << System::getErrString() <<
         std::endl;
      return false;
   }

   // + 20 is arbitary, but must be set for the linkTo size test below as well
   char* linkTo = (char *)malloc(MAX_LINK_SIZE  + 20);
   if (!linkTo)
   {
      std::cerr << "malloc failed (" << this->filePath <<"): " << System::getErrString() <<
         std::endl;
      return false;
   }

   // read linkTo and test its size
   int linkLength = readlinkat(this->dirFD, this->fileName.c_str(), linkTo, MAX_LINK_SIZE + 20);
   if (linkLength + 1 > MAX_LINK_SIZE ) // +1 as we also need to store the \0
   {
      std::cerr << "Symlink too long: " << this->filePath << std::endl;
      goto out;
   }

   linkTo[linkLength] = '\0'; // we need to add the closing character ourself

   if (linkLength < 0)
   {
      std::cerr << "Failed to read link: " << this->filePath << std::endl;
      goto out;
   }

   res = runCreateIoctl(&fromStatData, linkTo);
   if (res) {
      std::cerr << "Failed to create symlink (" << this->filePath << "): "
         << System::getErrString() << std::endl;
      goto out;
   }

   // only request a mode update if we had to remove some bits when we created the file
   if (fromStatData.st_mode & (S_ISVTX | S_ISUID | S_ISGID) )
   {
      metaRes = copyOwnerAndModeLink(&fromStatData); // mode and owner
      if (!static_cast<bool>(metaRes))
      {
         removeTmpOnErr();
         goto out;
      }
   }

   /* Just before the rename and to keep a possible race window a short as possible, we do a
    * sanity check, if the file was changed in the mean time */
   wasModified = this->fileWasModified(&fromStatData);
   if (wasModified)
   {
      removeTmpOnErr();
      goto out;
   }

   renameRes = renameat(this->dirFD, this->tmpName.c_str(), this->dirFD, this->fileName.c_str());
   if (renameRes)
   {
      std::cerr << "Renaming tmpFile failed (" << this->tmpName << "->" << this->filePath << "): "
         << System::getErrString() << std::endl;
      removeTmpOnErr();
      goto out;
   }

   // now copy over the time stamps, NOTE: although we renamed the file, tmpFD is still valid.
   metaRes = copyTimesLink(&fromStatData);
   if (!metaRes)
   {
      removeTmpOnErr();
      goto out;
   }

   retVal = true;

out:
   free(linkTo);
   return retVal;
}

/**
 * Check if the file we just restriped changed in the mean time
 *
 * @return    - true if the file has different stat data then in fromStatData
 */
bool MigrateFile::fileWasModified(struct stat* fromStatData)
{
   struct stat newStat;
   int statRes = fstatat(this->dirFD, this->fileName.c_str(), &newStat, AT_SYMLINK_NOFOLLOW);
   if (statRes)
   {
      if (errno != ENOENT)
         std::cerr << "Failed to stat file (" << this->filePath << "): "
            << System::getErrString() << std::endl;
      return true;
   }

   if (newStat.st_ino != fromStatData->st_ino)
   {
      std::cerr << "Inode changed during restripe (" << this->filePath << "): "
         << System::getErrString() << std::endl;
      return true;
   }

   if (newStat.st_mtime != fromStatData->st_mtime)
   {
      std::cerr << "File mtime was modified during restripe (" << this->filePath << "): "
         << System::getErrString() << std::endl;
      return true;
   }

   if (newStat.st_ctime != fromStatData->st_ctime)
   {
      std::cerr << "File ctime was modified during restripe (" << this->filePath << "): "
         << System::getErrString() << std::endl;
      return true;
   }

   if (newStat.st_size != fromStatData->st_size)
   {
      std::cerr << "File size changed during restripe (" << this->filePath << "): "
         << System::getErrString() << std::endl;
      return true;
   }

   if (newStat.st_nlink != fromStatData->st_nlink)
   {
      std::cerr << "Hardlink added during restripe (" << this->filePath << "): "
         << System::getErrString() << std::endl;
      return true;
   }

   return false;
}

/**
 * Remove the tmp file if any kind of critical error came up
 */
void MigrateFile::removeTmpOnErr()
{
   int unlinkRes = unlinkat(this->dirFD, this->tmpName.c_str(), 0);
   if (unlinkRes)
   {
      if (errno != ENOENT)
      {
         std::string tmpPath = this->dirPath + "/" + this->tmpName;
         std::cerr << "Unlinking of the tmp file (" << tmpPath << ") failed. Aborting. " <<
            "(" << System::getErrString() << ")" << std::endl;
         throw ModeMigrateException("Unexpected migration error!");
      }
   }
}

/**
 * Get the file descriptor and stat information from the file to be copied
 */
bool MigrateFile::getFromFileMeta(int *outFD, struct stat* outStatData)
{
   int flags = O_RDONLY | O_LARGEFILE | O_NOATIME | O_NOCTTY | O_NOFOLLOW;
   *outFD = openat(this->dirFD, this->fileName.c_str(), flags);
   if (*outFD < 0)
   {
      std::cerr << "Failed to open file to be copied (" << this->filePath << ") "
         << System::getErrString() << std::endl;
      return false;
   }

   int statRes = fstat(*outFD, outStatData);
   if (statRes < 0)
   {
      std::cerr << "Failed to stat file to be copied (" << this->filePath << "): "
         << System::getErrString() << std::endl;
      close(*outFD);
      return false;
   }
   return true;
}

/**
 * Create a temporary file using an ioctl call. We will also open the the new file.
 * We use an ioctl to create the file as we need to make sure about the used stripe targets
 *
 * @param symlinkTo only set for symlinks
 * @param mode only set for symlinks, defaults to 0
 * @return the file descriptor or -1 on error
 */
int MigrateFile::runCreateIoctl(struct stat* fromStat, const char* symlinkTo)
{
   uint16_t* prefTargets;
   int prefTargetsRawLen; // length of the prefTargets array in bytes (incl. terminating zero elem)

   bool prefTargetsRes = createPreferredTargetsArray(&prefTargets, &prefTargetsRawLen);
   if (!prefTargetsRes)
      return false;

   struct BeegfsIoctl_MkFileV3_Arg ioctlData;
   ::memset(&ioctlData, 0, sizeof(ioctlData) );

   int retVal = -1;

   ioctlData.ownerNodeID            = this->parentInfo.getOwnerNodeID().val();

   ioctlData.parentParentEntryID    = this->parentInfo.getParentEntryID().c_str();
   ioctlData.parentParentEntryIDLen = this->parentInfo.getParentEntryID().length() + 1;

   ioctlData.parentEntryID          = this->parentInfo.getEntryID().c_str();
   ioctlData.parentEntryIDLen       = this->parentInfo.getEntryID().length() + 1;

   ioctlData.parentName = "";
   ioctlData.parentNameLen = 1;

   ioctlData.parentIsBuddyMirrored  = this->parentInfo.getIsBuddyMirrored();

   /* file name of the temporary file. As we might exceed the limit of 255 chars,
    * we use a fixed string here, but appended with the thread-id to make it thread save */
   this->tmpName = BEEGFS_TMP_FILE_NAME + StringTk::intToStr(System::getTID() );
   ioctlData.entryName    = this->tmpName.c_str();
   ioctlData.entryNameLen = this->tmpName.length() + 1;
   ioctlData.fileType = this->fileType;

   /* as we set uid and gid directly on creating the file, there shouldn't be a security issue
    * additionally we mask the sbits */
   ioctlData.uid = fromStat->st_uid;
   ioctlData.gid = fromStat->st_gid;
   ioctlData.mode = fromStat->st_mode & ~(S_ISVTX | S_ISUID | S_ISGID);

   if (symlinkTo)
   {
      ioctlData.symlinkTo = symlinkTo;
      ioctlData.symlinkToLen = strlen(symlinkTo) + 1;
   }

   ioctlData.numTargets = ( (prefTargetsRawLen / sizeof(uint16_t) ) - 1); //remove the termination
   ioctlData.prefTargets = prefTargets;
   ioctlData.prefTargetsLen = prefTargetsRawLen;
   ioctlData.storagePoolId = this->storagePoolId.val();

   std::string newFile = this->dirPath + "/" + this->tmpName; // only for error messages

   IoctlTk ioctlTk(this->dirFD);
   bool ioctlRes = ioctlTk.createFile(&ioctlData);
   if (!ioctlRes)
   {
      std::cerr << "Failed to create file with ioctl: " << newFile << "; ";
      ioctlTk.printErrMsg();
      free(prefTargets);
      return -1;
   }

   if (this->fileType == DT_LNK)
   {
      // no need for the filedescriptor
      free(prefTargets);
      return 0;
   }

   // this is a regular file, now also open it...

   int flags = O_WRONLY | O_LARGEFILE | O_NOCTTY | O_NOFOLLOW;
   int tmpFD = openat(this->dirFD, this->tmpName.c_str(), flags, 0);
   if (tmpFD == -1)
   {
      std::cerr << "Failed to open the tmp file (" << newFile << ") : "
         << System::getErrString() << std::endl;
      int unlinkRes = unlinkat(this->dirFD, this->tmpName.c_str(), 0);
      if (unlinkRes && errno != ENOENT)
      {
         /* D'oh, we just successfully created a file, but failed to open it and now even
          * unlink() fails. Abort hard now! */
         std::cerr << "Failed to unlink the tmp file (" << newFile << ") : "
            << System::getErrString() << "; Aborting. " << std::endl;
         throw ModeMigrateException("Unexpected migration errror!");
      }
   }

   retVal = tmpFD;

   free(prefTargets);
   return retVal;
}


/**
 * Create the array of preferred storage targets.
 *
 * @param outPrefTargets will be alloc'ed, will be terminated with a 0 element
 * @param outPrefTargetsLen raw array length in bytes (incl. terminating 0 element)
 * @return false on error
 */
bool MigrateFile::createPreferredTargetsArray(uint16_t** outPrefTargets, int* outPrefTargetsLen)
{
   unsigned numPrefTargetsUsed = 0; // currently used/filled array elements (not raw bytes!)
   unsigned numPrefTargetsAlloced; // currently alloced array elements (not raw bytes!)


   /**
    *  alloc prefTargets array with reasonable size, the minimum of two times number of desired
    *  targets or the default value (20) but not more than available targets
    */
   if( this->desiredNumTargets >= (this->numDestTargets / 2) )
      numPrefTargetsAlloced = this->numDestTargets;
   else
   {
      numPrefTargetsAlloced = std::max(this->desiredNumTargets * 2,
         MIGRATEFILE_NUM_PREFERRED_TARGETS);
      numPrefTargetsAlloced = std::min(numPrefTargetsAlloced, this->numDestTargets);
   }


   // +1 for terminating 0 value
   uint16_t* prefTargets = (uint16_t*) malloc( ( numPrefTargetsAlloced + 1) * sizeof(uint16_t) );
   if(unlikely(!prefTargets) )
   {
      std::cerr << "Alloc of preferred targets array failed: "
         << System::getErrString() << std::endl;
      return false;
   }

   // copy stripe targets from vector
   // (first stripe target is random, the others follow continuously, wrapped around by modulo)
   int targetVecPos = MigrateFile::randomGen.getNextInRange(0, this->numDestTargets - 1);

   for (unsigned i = 0; i < numPrefTargetsAlloced; i++)
   {
      // current vector index, wrapped around at the end
      int currentVecPos = targetVecPos % this->numDestTargets;

      uint16_t currentTarget = this->destStorageTargetIDs->at(currentVecPos);

      // add current target to array

      prefTargets[numPrefTargetsUsed] = currentTarget;
      numPrefTargetsUsed++;

      // inc source target vector index

      targetVecPos++;
   }

   // append terminating 0 element
   prefTargets[numPrefTargetsUsed] = 0;
   numPrefTargetsUsed++;

   *outPrefTargets = prefTargets;
   *outPrefTargetsLen = numPrefTargetsUsed * sizeof(uint16_t);

   return true;
}

/**
 * Copy the content of the file
 */
bool MigrateFile::copyData(int fromFD, int tmpFD, struct stat* fromStatData)
{
   if (fromStatData->st_size == 0)
      return true; // file has a zero size, we won't do anything

   char* buffer = (char *)malloc(BUFFER_SIZE);
   if (!buffer)
   {
      std::cerr << this->filePath << ": Buffer allocation failed: " << System::getErrString() <<
         std::endl;
      return false;
   }

   char* nullBuffer = (char *)malloc(BUFFER_SIZE);
   if (!nullBuffer)
   {
      std::cerr << this->filePath << ": Buffer allocation failed: " << System::getErrString() <<
         std::endl;

      free(buffer);
      return false;
   }

   memset(nullBuffer, 0, BUFFER_SIZE);


   off_t readSizeLeft = fromStatData->st_size;
   errno = 0; // reset errno
   int readCount = 0;
   bool wasReAligned = false;
   bool wasSparse = false;
   while (readSizeLeft) // read loop
   {
      int readSize;

      if (readCount == 0 || readCount == BUFFER_SIZE || wasReAligned)
      {
         readSize = BEEGFS_MIN(BUFFER_SIZE, readSizeLeft);
         wasReAligned = false;
      }
      else {
         /* re-align to BUFFER_SIZE if read returned less than BUFFER-SIZE bytes (for example
          * after errno = EINTR). Will be important for sparse files, once we have 4K compares
          */
         readSize = BUFFER_SIZE - readCount;
         wasReAligned = true;
      }

      readCount = read(fromFD, buffer, readSize);

      if (readCount == 0) {
         std::cerr << "Unexpected end of file: " << this->filePath << std::endl;
         free(buffer);
         free(nullBuffer);
         return false;
      }

      if (readCount < 0)
      {
         if (errno == EINTR) // just ignore the interrupted call and continue to read
            continue;

         std::cerr << this->filePath << ": Failed to read file: " << System::getErrString() <<
            std::endl;
         free(buffer);
         free(nullBuffer);
         return false;
      }

      readSizeLeft -= readCount;

      // check for a sparse area,
      int memcmpRes = memcmp(nullBuffer, buffer, readCount);
      if (memcmpRes == 0)
      {
         // so it is sparse and we can simply seek
         wasSparse = true;

         off_t oldPos = lseek(tmpFD, 0, SEEK_CUR);
         if (oldPos >= 0)
         {
            off_t seekRes = lseek(tmpFD, readCount, SEEK_CUR); // seek ahead by readCount
            if (seekRes > 0)
               continue;

            // So SEEK_SET failed, but what is our position now? Lets try to go back to oldPos
            seekRes = lseek(tmpFD, oldPos, SEEK_SET);
            if (seekRes < 0)
            {
               // Now everything failed!
               std::cerr << "Error, lseek for sparse file failed, skipping: "
                  << this->filePath << std::endl;
               free(buffer);
               free(nullBuffer);
               return false;
            }
         }
      }

      wasSparse = false; // reset sparse
      int writeCount = 0;
      while (writeCount < readCount) // write loop
      {
         writeCount += write(tmpFD, buffer, readCount - writeCount);

         if (writeCount < 0)
         {
            if (errno == EINTR) // just ignore the interrupted call and continue to write
               continue;

            std::cerr << this->filePath << ": Failed to read file: "
               << System::getErrString() << std::endl;
            free(buffer);
            free(nullBuffer);
            return false;
         }
      }
   }

   bool retVal = true; // so far a success

   if (wasSparse)
   {
      /* At least the last chunk of the file was sparse and we did not write to it. Now we need
       * to set the correct file size */
      int truncRes = ftruncate(tmpFD, fromStatData->st_size);
      if (unlikely(truncRes) )
      {  /* ftruncate is allowed to fail as we are truncating up the file. Unfortunately,
          * posix does not say which error this is supposed to be, so we can't test for this
          * specific error.*/

         // We now write a single byte over the file size and then truncate back

         const char* buf = "abc";

         int writeRes = pwrite(tmpFD, buf, 1, fromStatData->st_size); // write a single by over size
         if (unlikely(writeRes) )
         {
            std::cerr << "Sparse-write tmp file to file-size + 1 failed!" << std::endl;
            retVal = false;
         }
         else
         {
            truncRes = ftruncate(tmpFD, fromStatData->st_size);
            if (truncRes)
            {
               std::cerr << "Sparse truncating down tmp file to the correct file size failed. "
                  << std::endl;
               retVal = false;
            }
         }

      }
   }

   free(buffer);
   free(nullBuffer);

   return retVal;
}


#if ((_POSIX_C_SOURCE) >= 200809L)
/**
 * Copy over the time stamp. Note: Call this after renaming the file from the tmp-filename to the
 * original-file-name, as the rename operation also might update time stamps
 */
bool MigrateFile::copyTimes(int fd, struct stat* fromStatData)
{
   struct timespec timeValues[2];

   timeValues[0].tv_sec = fromStatData->st_atime;
   timeValues[0].tv_nsec = 0; // we don't have it

   timeValues[1].tv_sec = fromStatData->st_mtime;
   timeValues[1].tv_nsec = 0; // we don't have it

   int setTimeRes = futimens(fd, timeValues);
   if (setTimeRes) {
      std::cerr << "Updating the time stamps failed (" << this->filePath << "): "
         << System::getErrString() << std::endl;
      return false;
   }

   return true;
}

/**
 * Copy over the time stamp. Note: Call this after renaming the file from the tmp-filename to the
 * original-file-name, as the rename operation also might update time stamps
 * NOTE: for symlinks, as we don't have the tmp-file-descriptor for those
 */
bool MigrateFile::copyTimesLink(struct stat* fromStatData)
{
   struct timespec timeValues[2];

   timeValues[0].tv_sec = fromStatData->st_atime;
   timeValues[0].tv_nsec = 0; // we don't have it

   timeValues[1].tv_sec = fromStatData->st_mtime;
   timeValues[1].tv_nsec = 0; // we don't have it

   int setTimeRes = utimensat(this->dirFD, this->fileName.c_str(), timeValues, AT_SYMLINK_NOFOLLOW);
   if (setTimeRes) {
      std::cerr << "Updating the time stamps failed (" << this->filePath << "): "
         << System::getErrString() << std::endl;
      return false;
   }

   return true;
}
#else
/**
 * Copy over the time stamp. Note: Call this after renaming the file from the tmp-filename to the
 * original-file-name, as the rename operation also might update time stamps
 */
bool MigrateFile::copyTimes(int fd, struct stat* fromStatData)
{
   struct timeval timeValues[2];

   timeValues[0].tv_sec = fromStatData->st_atime;
   timeValues[0].tv_usec = 0; // we don't have it

   timeValues[1].tv_sec = fromStatData->st_mtime;
   timeValues[1].tv_usec = 0; // we don't have it

   int setTimeRes = futimes(fd, timeValues); // NOTE: futimes is Linux specific
   if (setTimeRes) {
      std::cerr << "Updating the time stamps failed (" << this->filePath << "): "
         << System::getErrString() << std::endl;
      return false;
   }

   return true;
}

/**
 * Copy over the time stamp. Note: Call this after renaming the file from the tmp-filename to the
 * original-file-name, as the rename operation also might update time stamps
 * NOTE: for symlinks, as we don't have the tmp-file-descriptor for those
 */
bool MigrateFile::copyTimesLink(struct stat* fromStatData)
{
   // We simply cannot set time stamps for symlinks on those distributions, which do not have
   // utimensat() yet
   return true;
}

#endif

/**
 * Copy over owner and mode to the tmpFile
 */
bool MigrateFile::copyOwnerAndMode(int tmpFD, struct stat* fromStatData)
{
   /* NOTE: Set the owner first and only then the mode, as otherwise there will be a brief
            security issue (e.g. s-bit is set... */
   int chownRes = fchown(tmpFD, fromStatData->st_uid, fromStatData->st_gid);
   if (chownRes)
   {
      std::cerr << "Setting the owner failed (" << this->filePath << "): "
         << System::getErrString() << std::endl;
      return false;
   }

   int chmodRes = fchmod(tmpFD, fromStatData->st_mode);
   if (chmodRes)
   {
      std::cerr << "Setting the file mode failed (" << this->filePath << "): "
         << System::getErrString() << std::endl;
      return false;
   }

   return true;
}

/**
 * Copy over owner and mode to the tmpFile (link)
 * NOTE: for symlinks, as we don't have the tmp-file-descriptor for those
 */
bool MigrateFile::copyOwnerAndModeLink(struct stat* fromStatData)
{
   /* NOTE: Set the owner first and only then the mode, as otherwise there will be a brief
            security issue (e.g. s-bit is set... */
   int chownRes = fchownat(this->dirFD, this->tmpName.c_str(), fromStatData->st_uid,
      fromStatData->st_gid, AT_SYMLINK_NOFOLLOW);
   if (chownRes)
   {
      std::cerr << "Setting the owner failed (" << this->filePath << "): "
         << System::getErrString() << std::endl;
      return false;
   }

#if 0
   int chmodRes = fchmodat(this->dirFD, this->tmpName.c_str(), 0644,
      AT_SYMLINK_NOFOLLOW);
   if (chmodRes)
   {
      std::cerr << "Setting the file mode failed (" << this->filePath << "): "
         << System::getErrString() << std::endl;
      return false;
   }
#endif // fchmodat(...,..., mode, AT_SYMLINK_NOFOLLOW) not (yet) supported in linux

   return true;
}

namespace
{
   /**
    * Format and print an extended attribute error message.
    *
    * @param exception The extended attribute exception to be printed.
    */
   void printError(const XAttrException& exception)
   {
      std::cerr << exception.what() << std::endl <<

         "Make sure the file system is not in use during migration. " <<
         "Also, check the consistency of the " <<
         "file system with beegfs-fsck and check the log files for error messages." << std::endl;
   }
}

/**
 * Get all extended attributes names and their respective values.
 *
 * @param outXAttrs The output map of extended attributes of the file to be copied.
 *                  The map contains the attribute names and their respective values.
 *                  Each value is a tuple containing the attribute value array and its length.
 * @return true if the map could be retrieved; false otherwise.
 */
boost::optional<XAttrMap> MigrateFile::getXAttrs() const
{
   try
   {
      return XAttrTk::getXAttrs(this->filePath);
   }
   catch (const XAttrException& exception)
   {
      printError(exception);
      return boost::none;
   }
}

/**
 * Sets extended attributes of migrated file.
 *
 * @param fromXAttrs A map of extended attributes names and their respective values.
 *                   Each map object is a tuple containing the attribute value array
 *                   and its length.
 * @return true if the copy was successful; false otherwise.
 */
bool MigrateFile::copyXAttrs(const XAttrMap& fromXAttrs)
{
   try
   {
      XAttrTk::setXAttrs(this->filePath, fromXAttrs);
   }
   catch (const XAttrException& exception)
   {
      printError(exception);
      return false;
   }
   return true;
}
