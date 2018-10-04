#include <common/app/log/LogContext.h>
#include <common/toolkit/StorageTk.h>
#include <common/toolkit/StringTk.h>
#include "System.h"

#include <mutex>

#include <sys/utsname.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>


Mutex System::strerrorMutex;

uid_t System::savedEffectiveUID = geteuid();
gid_t System::savedEffectiveGID = getegid();


std::string System::getErrString()
{
   int errcode = errno;

   const std::lock_guard<Mutex> lock(strerrorMutex);

   return strerror(errcode);
}

std::string System::getErrString(int errcode)
{
   const std::lock_guard<Mutex> lock(strerrorMutex);

   return strerror(errcode);
}

std::string System::getHostname()
{
   struct utsname utsnameBuf;

   uname(&utsnameBuf);

   std::string hostname(utsnameBuf.nodename);

   return hostname;
}

int System::getNumOnlineCPUs()
{
   long confRes = sysconf(_SC_NPROCESSORS_ONLN);
   if(confRes <= 0)
      throw InvalidConfigException("Unable to query number of online CPUs via sysconf()");

   return confRes;
}

/**
 * Returns the number of numa nodes by reading /sys/devices/system/node/nodeXY.
 *
 * @return number of NUMA nodes; this has no error return value (to keep the calling code simple)
 * if an error occurs (e.g. the numa check path not exists) then this will return 1 and a warning
 * will be printed to log.
 */
int System::getNumNumaNodes()
{
   const char* logContext = "System (get number of NUMA nodes)";
   static bool errorLogged = false; // to avoid log spamming on error

   const char* path = "/sys/devices/system/node";
   const char* searchEntry = "node";
   size_t searchEntryStrLen = strlen(searchEntry);

   int numNumaNodes = 0;

   if(!StorageTk::pathExists(path) )
   {
      if(!errorLogged)
         LogContext(logContext).log(Log_WARNING,
            "NUMA node check path not found: " + std::string(path) + ". "
            "Assuming single NUMA node.");

      errorLogged = true;
      return 1;
   }

   StringList pathEntries; // path dir entries

   StorageTk::readCompleteDir(path, &pathEntries);

   for(StringListIter iter = pathEntries.begin(); iter != pathEntries.end(); iter++)
   {
      if(!strncmp(searchEntry, iter->c_str(), searchEntryStrLen) )
         numNumaNodes++; // found a numa node
   }

   if(!numNumaNodes)
   {
      if(!errorLogged)
         LogContext(logContext).log(Log_WARNING,
            "No NUMA nodes found in path: " + std::string(path) + ". "
            "Assuming single NUMA node.");

      errorLogged = true;
      return 1;
   }

   return numNumaNodes;
}

/**
 * Returns the CPU cores that belong to the given numa node.
 *
 * @param nodeNum number of the numa node (as seen in sys/devices/system/node/nodeXY).
 * @param outCpuSet will be initialized within this method.
 * @return number of CPU cores that belong to the given NUMA node (may be 0 if an error occurs, e.g.
 * NUMA core check path not exists).
 */
int System::getNumaCoresByNode(int nodeNum, cpu_set_t* outCpuSet)
{
   const char* logContext = "System (get NUMA cores by node)";
   static bool errorLogged = false; // to avoid log spamming on error

   std::string path = "/sys/devices/system/node/node" + StringTk::intToStr(nodeNum);
   const char* searchEntry = "cpu";
   size_t searchEntryStrLen = strlen(searchEntry);

   int numCores = 0; // detected number of cores for the given numa node

   CPU_ZERO(outCpuSet); // initialize the set

   if(!StorageTk::pathExists(path) )
   {
      if(!errorLogged)
         LogContext(logContext).log(Log_WARNING, "NUMA core check path not found: " + path);

      errorLogged = true;
      return 0;
   }

   StringList pathEntries; // path dir entries

   StorageTk::readCompleteDir(path.c_str(), &pathEntries);

   for(StringListIter iter = pathEntries.begin(); iter != pathEntries.end(); iter++)
   {
      if(!strncmp(searchEntry, iter->c_str(), searchEntryStrLen) )
      { // found a core for this numa node
         numCores++;

         std::string coreNumStr = std::string(iter->c_str() ).substr(searchEntryStrLen);

         // note: there are other entries that start with "cpu" as well, so we check for digits:
         if(coreNumStr.empty() || !isdigit(coreNumStr[0] ) )
            continue;

         int coreNum = StringTk::strToInt(coreNumStr);

         CPU_SET(coreNum, outCpuSet);
      }
   }

   if(!numCores)
   {
      if(!errorLogged)
         LogContext(logContext).log(Log_WARNING, "No cores found in path: " + path);

      errorLogged = true;
      return 0;
   }

   return numCores;
}

/**
 * Set affinity of current process to given NUMA zone.
 *
 * @param nodeNum zero-based
 */
bool System::bindToNumaNode(int nodeNum)
{
   const char* logContext = "PThread (start on NUMA node)";
   static bool errorLogged = false; // to avoid log spamming on error

   // init cpuSet with cores of give numa node

   cpu_set_t cpuSet;

   int numCores = System::getNumaCoresByNode(nodeNum, &cpuSet);
   if(!numCores)
   { // something went wrong with core retrieval, so fall back to running on all cores
      if(!errorLogged)
         LogContext(logContext).log(Log_WARNING,
            "Failed to detect CPU cores for NUMA zone. Falling back to allowing all cores. "
            "Failed zone: " + StringTk::intToStr(nodeNum) );

      errorLogged = true;
      return false;
   }

   int affinityRes = sched_setaffinity(getPID(), sizeof(cpuSet), &cpuSet);
   if(affinityRes)
   { // failed to set affinity
      if(!errorLogged)
         LogContext(logContext).log(Log_WARNING,
            "Failed to set process affinity to NUMA zone. "
            "Failed zone: " + StringTk::intToStr(nodeNum) + "; "
            "SysErr: " + System::getErrString() );

      errorLogged = true;
      return false;
   }

   return true;
}

/**
 * @return linux thread ID (this is not the POSIX thread ID!)
 */
pid_t System::getTID()
{ // note: gettid() man page says "use syscall()" for this
   return syscall(SYS_gettid);
}

/**
 * Increase the process limit for number of open file descriptors (also includes sockets etc).
 *
 * Note: This is increase only, so it will do nothing if the new limit is lower than the old one.
 *
 * @param outOldLimit may be NULL
 * @return false on error (and errno is set on error)
 */
bool System::incProcessFDLimit(uint64_t newLimit, uint64_t* outOldLimit)
{
   const char* logContext = "Increase process FD limit";

   struct rlimit oldRLimitData;

   int getLimitRes = getrlimit(RLIMIT_NOFILE, &oldRLimitData);
   if(getLimitRes == -1)
   {
      LogContext(logContext).log(Log_WARNING,
         std::string("Unable to get rlimit for number of files. SysErr: ") +
         System::getErrString() );

      if(outOldLimit)
         *outOldLimit = 0;

      return false;
   }
   else
   if(newLimit > oldRLimitData.rlim_cur)
   { // check current limit and raise if necessary
      struct rlimit newRLimitData = oldRLimitData;

      newRLimitData.rlim_cur = BEEGFS_MAX(newLimit, oldRLimitData.rlim_cur);
      newRLimitData.rlim_max = BEEGFS_MAX(newLimit, oldRLimitData.rlim_max);

      LOG_DEBUG(logContext, Log_SPAM,
         std::string("Setting rlimit for number of files... ") +
         std::string("soft old: ") + StringTk::uint64ToStr(oldRLimitData.rlim_cur) + "; " +
         std::string("soft new: ") + StringTk::uint64ToStr(newRLimitData.rlim_cur) + "; " +
         std::string("hard old: ") + StringTk::uint64ToStr(oldRLimitData.rlim_max) + "; " +
         std::string("hard new: ") + StringTk::uint64ToStr(newRLimitData.rlim_max) );

      if(outOldLimit)
         *outOldLimit = oldRLimitData.rlim_cur;

      int setLimitRes = setrlimit(RLIMIT_NOFILE, &newRLimitData);
      if(setLimitRes == -1)
         return false;
   }

   return true;
}

/**
 * Get information about memory usage on the current system (in bytes).
 *
 * Note: if something goes wrong values will be set to 0.
 */
void System::getMemoryInfo(uint64_t *memTotal, uint64_t *memFree, uint64_t *memCached,
   uint64_t *memBuffered)
{
   *memTotal = 0;
   *memFree = 0;
   *memCached = 0;
   *memBuffered = 0;

   std::string token;
   std::ifstream file("/proc/meminfo");
   while (file >> token)
   {
      if(token == "MemTotal:")
      {
         uint64_t mem = 0;
         if(file >> mem)
         {
            *memTotal = mem * 1024;
         }
         else
         {
            *memTotal = 0;
         }
      }
      else
      if(token == "MemFree:")
      {
         uint64_t mem = 0;
         if(file >> mem)
         {
            *memFree = mem * 1024;
         }
         else
         {
            *memFree = 0;
         }
      }
      else
      if(token == "Cached:")
      {
         uint64_t mem = 0;
         if(file >> mem)
         {
            *memCached = mem * 1024;
         }
         else
         {
            *memCached = 0;
         }
      }
      else
      if(token == "Buffers:")
      {
         uint64_t mem = 0;
         if(file >> mem)
         {
            *memBuffered = mem * 1024;
         }
         else
         {
            *memBuffered = 0;
         }
      }

      // ignore rest of the line
      file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
   }
}

/**
 * Get the amount of usable memory (free+cached) in Byte
 *
 * @return the amount of usable memory in Byte
 */
uint64_t System::getUsableMemorySize()
{
   uint64_t memTotal = 0;
   uint64_t memFree = 0;
   uint64_t memCached = 0;
   uint64_t memBuffered = 0;

   getMemoryInfo(&memTotal, &memFree, &memCached, &memBuffered);

   return memFree+memCached;
}

/*
 * resolves the given UID (user ID) to the user name.
 *
 * WARNING: this is a non-reentrant version (libc function without _r suffix), so don't use it
 * multi-threaded.
 *
 * @param uid system user ID
 * @return user name or empty string if user not found
 */
std::string System::getUserNameFromUID(unsigned uid)
{
   std::string userName;
   struct passwd* userData = getpwuid(uid);

   if(userData != NULL)
      userName = userData->pw_name;

   return userName;
}

/*
 * resolves the given username to its numeric UID (user ID).
 *
 * WARNING: this is a non-reentrant version (libc function without _r suffix), so don't use it
 * multi-threaded.
 *
 * @param system username
 * @return false if user not found
 */
bool System::getUIDFromUserName(std::string username, uid_t* outUID)
{
   struct passwd* userData = getpwnam(username.c_str() );

   if(!userData)
      return false;

   *outUID = userData->pw_uid;
   return true;
}


/*
 * resolves the given GID (group ID) to the group name
 *
 * WARNING: this is a non-reentrant version (libc function without _r suffix), so don't use it
 * multi-threaded.
 *
 * @param gid system group ID
 * @return group name or empty string if group not found
 */
std::string System::getGroupNameFromGID(unsigned gid)
{
   std::string groupName;
   struct group* groupData = getgrgid(gid);

   if(groupData != NULL)
      groupName = groupData->gr_name;

   return groupName;
}

/*
 * resolves the given groupname to its numeric GID (group ID).
 *
 * WARNING: this is a non-reentrant version (libc function without _r suffix), so don't use it
 * multi-threaded.
 *
 * @param system groupname.
 * @return false if group not found.
 */
bool System::getGIDFromGroupName(std::string groupname, gid_t* outGID)
{
   struct group* groupData = getgrnam(groupname.c_str() );

   if(!groupData)
      return false;

   *outGID = groupData->gr_gid;
   return true;
}


/*
 * collects all IDs of the users which are available on the system.
 *
 * WARNING: this is a non-reentrant version (libc function without _r suffix), so don't use it
 * multi-threaded.
 *
 * @param outUserIDs the UIDs of the known users
 * @param ignoreSystemUsers if true, ignores all UIDs lower than 100 and user with the default
 * shell "/sbin/nologin" or "/usr/sbin/nologin" or "/bin/false"
 */
void System::getAllUserIDs(UIntList* outUserIDs, bool ignoreSystemUsers)
{
   struct passwd* userData;

   userData = getpwent();

   while(userData)
   {
      if(!ignoreSystemUsers)
         outUserIDs->push_back(userData->pw_uid);
      else
      if( (userData->pw_uid > 100) &&
          strcmp(userData->pw_shell, "/sbin/nologin") != 0 &&
          strcmp(userData->pw_shell, "/usr/sbin/nologin") != 0 &&
          strcmp(userData->pw_shell, "/bin/false") != 0 )
         outUserIDs->push_back(userData->pw_uid);

      userData = getpwent();
   }

   //close the user DB
   endpwent();
}

/*
 * collects all IDs of the groups which are available on the system,
 *
 * WARNING: this is a non-reentrant version (libc function without _r suffix), so don't use it
 * multi-threaded.
 *
 * @param outGroupIDs the GIDs of the known groups
 * @param ignoreSystemGroups if true, ignores all GIDs lower than 100
 */
void System::getAllGroupIDs(UIntList* outGroupIDs, bool ignoreSystemGroups)
{
   struct group* groupData;

   groupData = getgrent();

   while(groupData)
   {
      if(!ignoreSystemGroups || (groupData->gr_gid > 100) )
         outGroupIDs->push_back(groupData->gr_gid);

      groupData = getgrent();
   }

   //close the group DB
   endgrent();
}

/*
 * set the UID and the GID for file-system access, the following file system access of the thread
 * will be done with the given user and group, this method must be called again with the previousUID
 * and previousGID to reset the UID and GID
 *
 * @param uid user ID to use for the file system operations which will be done after this method
 * @param gid group ID to use for the file system operations which will be done after this method
 * @param outPreviousUID the user ID which was used before this method was called (return value)
 * @param outPreviousGID the group ID which was used before this method was called (return value)
 */
void System::setFsIDs(unsigned uid, unsigned gid, unsigned *outPreviousUID,
   unsigned *outPreviousGID)
{
   *outPreviousUID = setfsuid(uid);
   *outPreviousGID = setfsgid(gid);
}

/**
 * Drop effective user and group ID by setting it to real user and group ID.
 *
 * Note: FS UID/GID will also implicitly be set to this value. However, POSIX does not require to
 * permit setting eUID to the current eUID value, so this method might not work after
 * elevateEffectiveUserAndGroupFsID().
 *
 * @return true on success
 */
bool System::dropUserAndGroupEffectiveID()
{
   int setUIDRes = seteuid(getuid() );
   int setGIDRes = setegid(getgid() );

   return (!setUIDRes && !setGIDRes);
}

/**
 * Elevate the effective user and group FS ID to the saved (originally effective) user and group ID.
 *
 * Note: You probably want to call setFsIDs() when you are through with the privileged code part.
 */
void System::elevateUserAndGroupFsID(unsigned* outPreviousUID, unsigned* outPreviousGID)
{
   setFsIDs(savedEffectiveUID, savedEffectiveGID, outPreviousUID, outPreviousGID);
}
