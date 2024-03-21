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
bool IoctlTk::createFile(BeegfsIoctl_MkFileV3_Arg* fileData)
{
   int res = ioctl(this->fd, BEEGFS_IOC_CREATE_FILE_V3, fileData);
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
bool IoctlTk::testIsFhGFS()
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

bool IoctlTk::getInodeID(const std::string& entryID, uint64_t& outInodeID)
{
   bool res = beegfs_getInodeID(fd, entryID.c_str(), &outInodeID);

   if(!res)
   {
      this->errorMsg = strerror(errno);
      return false;
   }

   return true;
}

bool IoctlTk::getEntryInfo(EntryInfo& outEntryInfo)
{
   uint32_t ownerID;
   std::vector<char> parentEntryID(BEEGFS_IOCTL_ENTRYID_MAXLEN + 1);
   std::vector<char> entryID(BEEGFS_IOCTL_ENTRYID_MAXLEN + 1);
   int entryType;
   int featureFlags;

   bool res = beegfs_getEntryInfo(fd, &ownerID, parentEntryID.data(), entryID.data(),
         &entryType, &featureFlags);
   if(!res)
   {
      this->errorMsg = strerror(errno);
      return false;
   }

   outEntryInfo.set(NumNodeID(ownerID), parentEntryID.data(), entryID.data(), "",
         (DirEntryType)entryType, featureFlags);

   return true;
}

bool IoctlTk::pingNode(BeegfsIoctl_PingNode_Arg* inoutPing)
{
   bool res = beegfs_pingNode(fd, inoutPing);
   if(!res)
   {
      this->errorMsg = strerror(errno);
      return false;
   }
   return true;
}

