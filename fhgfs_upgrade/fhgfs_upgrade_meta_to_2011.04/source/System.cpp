#include "StringTk.h"
#include "System.h"

#include <sys/utsname.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/resource.h>


std::string System::getErrString()
{
   return strerror(errno);
}

std::string System::getErrString(int errcode)
{
   return strerror(errcode);
}

std::string System::getHostname()
{
   struct utsname utsnameBuf;

   uname(&utsnameBuf);

   std::string hostname(utsnameBuf.nodename);

   return hostname;
}

/**
 * @return <=0 on error
 */
long System::getNumOnlineCPUs()
{
   return sysconf(_SC_NPROCESSORS_ONLN);
}

/**
 * @return linux thread ID (this is not the POSIX thread ID!)
 */
pid_t System::getTID()
{ // note: gettid() man page says "use syscall()" for this
   return syscall(SYS_gettid);
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

