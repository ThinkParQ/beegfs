#ifndef IOCTLTK_H_
#define IOCTLTK_H_

#include <common/Common.h>
#include <common/system/System.h>
#include <beegfs/beegfs.h>
#include <common/nodes/NumNodeID.h>
#include <common/storage/EntryInfo.h>


struct BeegfsIoctl_MkFile_Arg; // forward declaration
struct BeegfsIoctl_PingNode_Arg;

class IoctlTk
{
   public:
      /**
       * Note: Use isFDValid() afterwards to check if open() was successful.
       */
      IoctlTk(std::string path)
      {
         this->fd = open(path.c_str(), O_RDONLY );
         if (this->fd < 0)
            this->errorMsg = "Failed to open " + path + ": " + System::getErrString(errno);
      }

      IoctlTk(int fd)
      {
         if (fd < 0)
         {
            this->errorMsg = "Invalid file descriptor given";
            this->fd = -1;
            return;
         }

         this->fd = dup(fd);
         if(this->fd < 0)
         {
            this->errorMsg = "Duplicating the file descriptor failed: " +
               System::getErrString(errno);
         }
      }

      ~IoctlTk(void)
      {
         if (this->fd >= 0)
            close(this->fd);
      }

      bool getMountID(std::string* outMountID);
      bool getCfgFile(std::string* outCfgFile);
      bool getRuntimeCfgFile(std::string* outCfgFile);
      bool createFile(BeegfsIoctl_MkFileV3_Arg* fileData);
      bool testIsFhGFS(void);
      bool getStripeInfo(unsigned* outPatternType, unsigned* outChunkSize, uint16_t* outNumTargets);
      bool getStripeTarget(uint32_t targetIndex, BeegfsIoctl_GetStripeTargetV2_Arg& outInfo);
      bool mkFileWithStripeHints(std::string filename, mode_t mode, unsigned numtargets,
         unsigned chunksize);
      bool getInodeID(const std::string& entryID, uint64_t& outInodeID);
      bool getEntryInfo(EntryInfo& outEntryInfo);
      bool pingNode(BeegfsIoctl_PingNode_Arg* inoutPing);


   private:
      int fd; // file descriptor
      std::string errorMsg; // if something fails


   public:
      // inliners

      inline bool isFDValid(void)
      {
         if (this->fd < 0)
            return false;

         return true;
      }

      /**
       * Print error message of failures.
       */
      inline void printErrMsg(void)
      {
         std::cerr << this->errorMsg << std::endl;
      }

      /**
       * Get last error message.
       */
      std::string getErrMsg()
      {
         return this->errorMsg;
      }
};

#endif /* IOCTLTK_H_ */
