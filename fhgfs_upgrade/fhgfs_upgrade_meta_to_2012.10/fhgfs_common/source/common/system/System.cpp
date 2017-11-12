#include <common/app/log/LogContext.h>
#include <common/threading/SafeMutexLock.h>
#include <common/toolkit/StorageTk.h>
#include <common/toolkit/StringTk.h>
#include "System.h"

#include <sys/utsname.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/resource.h>


Mutex System::strerrorMutex;


std::string System::getErrString()
{
   int errcode = errno;

   SafeMutexLock mutexLock(&strerrorMutex);

   std::string errString = strerror(errcode);
   
   mutexLock.unlock();
   
   return errString;
}

std::string System::getErrString(int errcode)
{
   SafeMutexLock mutexLock(&strerrorMutex);
   
   std::string errString = strerror(errcode);
   
   mutexLock.unlock();
   
   return errString;
}

std::string System::getHostname()
{
   struct utsname utsnameBuf;
   
   uname(&utsnameBuf);
   
   std::string hostname(utsnameBuf.nodename);
   
   return hostname;   
}

int System::getNumOnlineCPUs() throw(InvalidConfigException)
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

      newRLimitData.rlim_cur = FHGFS_MAX(newLimit, oldRLimitData.rlim_cur);
      newRLimitData.rlim_max = FHGFS_MAX(newLimit, oldRLimitData.rlim_max);

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

