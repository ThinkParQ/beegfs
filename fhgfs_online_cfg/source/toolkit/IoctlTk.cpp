#include <sys/types.h>
#include <sys/stat.h>
#include <linux/fs.h>

#include "IoctlTk.h"


/**
 * Get the mountID aka clientID aka nodeID of a client aka sessionID.
 */
bool IoctlTk::getMountID(std::string* outMountID)
{
   char* mountID;

   bool getRes = beegfs_getMountID(fd, &mountID);
   if(!getRes || !mountID)
   {
      this->errorMsg = strerror(errno);
      return false;
   }

   *outMountID = mountID;
   free(mountID);

   return true;
}

/**
 * ioctl to get the path to a client config file.
 */
bool IoctlTk::getCfgFile(std::string* outCfgFile)
{
   char* cfgFile;

   bool getRes = beegfs_getConfigFile(fd, &cfgFile);
   if(!getRes || !cfgFile)
   {
      this->errorMsg = strerror(errno);
      return false;
   }

   *outCfgFile = cfgFile;
   free(cfgFile);

   return true;
}

/**
 * ioctl to get the path to a client config file from the proc directory.
 */
bool IoctlTk::getRuntimeCfgFile(std::string* outCfgFile)
{
   char* cfgFile;

   bool getRes = beegfs_getRuntimeConfigFile(fd, &cfgFile);
   if(!getRes || !cfgFile)
   {
      this->errorMsg = strerror(errno);
      return false;
   }

   *outCfgFile = cfgFile;
   free(cfgFile);

   return true;
}

/**
 * ioctl to create a file with special attributes.
 */
bool IoctlTk::createFile(BeegfsIoctl_MkFile_Arg* fileData)
{
   int res = ioctl(this->fd, BEEGFS_IOC_CREATE_FILE, fileData);
   if (res)
   {
      this->errorMsg = strerror(errno);
      return false;
   }

   return true;
}

/**
 * ioctl to test if the underlying fs is a fhgfs.
 */
bool IoctlTk::testIsFhGFS(void)
{
   bool testRes = beegfs_testIsBeeGFS(fd);
   if(!testRes)
   {
      this->errorMsg = strerror(errno);
      return false;
   }

   return true;
}

/**
 * ioctl to get the stripe info of a file (chunksize etc.).
 */
bool IoctlTk::getStripeInfo(unsigned* outPatternType, unsigned* outChunkSize,
   uint16_t* outNumTargets)
{
   bool getRes = beegfs_getStripeInfo(fd, outPatternType, outChunkSize, outNumTargets);

   if(!getRes)
   {
      this->errorMsg = strerror(errno);
      return false;
   }

   return true;
}

/**
 * ioctl to get the stripe target of a file (via 0-based index).
 */
bool IoctlTk::getStripeTarget(uint32_t targetIndex, BeegfsIoctl_GetStripeTargetV2_Arg& outInfo)
{
   return beegfs_getStripeTargetV2(fd, targetIndex, &outInfo);
}

bool IoctlTk::mkFileWithStripeHints(std::string filename, mode_t mode, unsigned numtargets,
   unsigned chunksize)
{
   bool mkRes = beegfs_createFile(fd, filename.c_str(), mode, numtargets, chunksize);

   if(!mkRes)
   {
      this->errorMsg = strerror(errno);
      return false;
   }

   return true;
}
