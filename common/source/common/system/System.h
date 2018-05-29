#ifndef SYSTEM_H_
#define SYSTEM_H_

#include <common/Common.h>
#include <common/app/config/InvalidConfigException.h>

#include <sys/fsuid.h>
#include <limits>


class Mutex; // forward declaration


class System
{
   public:
      static std::string getErrString();
      static std::string getErrString(int errcode);
      static std::string getHostname();
      static int getNumOnlineCPUs();
      static int getNumNumaNodes();
      static int getNumaCoresByNode(int nodeNum, cpu_set_t* outCpuSet);
      static bool bindToNumaNode(int nodeNum);
      static pid_t getTID();
      static bool incProcessFDLimit(uint64_t newLimit, uint64_t* outOldLimit);
      static void getMemoryInfo(uint64_t *memTotal, uint64_t *memFree, uint64_t *memCached,
         uint64_t *memBuffered);
      static uint64_t getUsableMemorySize();

      static std::string getUserNameFromUID(unsigned uid);
      static bool getUIDFromUserName(std::string username, uid_t* outUID);
      static std::string getGroupNameFromGID(unsigned gid);
      static bool getGIDFromGroupName(std::string groupname, gid_t* outGID);
      static void getAllUserIDs(UIntList* outUserIDs, bool ignoreSystemUsers);
      static void getAllGroupIDs(UIntList* outGroupIDs, bool ignoreSystemGroups);

      static void setFsIDs(unsigned uid, unsigned gid, unsigned *outPreviousUID,
         unsigned *outPreviousGID);
      static bool dropUserAndGroupEffectiveID();
      static void elevateUserAndGroupFsID(unsigned* outPreviousUID, unsigned* outPreviousGID);


   private:
      System()
      {
      }

      static Mutex strerrorMutex;

      static uid_t savedEffectiveUID;
      static gid_t savedEffectiveGID;


   public:
      // inliners

      /**
       * @return process ID
       */
      static pid_t getPID()
      {
         return getpid();
      }

      /**
       * @return POSIX thread ID, unsigned long (this is not the linux thread ID)
       */
      static pthread_t getPosixTID()
      {
         return pthread_self();
      }

      static uint64_t getCurrentTimeSecs()
      {
         return time(NULL);
      }

};

#endif /*SYSTEM_H_*/
