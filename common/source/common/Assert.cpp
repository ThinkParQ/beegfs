#include "Common.h"

#ifdef BEEGFS_DEBUG

#include <common/app/AbstractApp.h>
#include <common/app/log/Logger.h>

#include <execinfo.h>
#include <sstream>

namespace beegfs_debug
{

void assertMsg(const char* file, unsigned line, const char* condition)
{
   std::stringstream message;

   const char* basename = strrchr(file, '/');
   if (!basename)
      basename = file;
   else
      basename++;

   message << "ASSERTION TRIGGERED AT " << basename << ":" << line << '\n';

   message << '\n';
   message << condition;
   message << '\n';

   message << '\n';
   message << "Backtrace of asserting thread ";
   message << PThread::getCurrentThreadName();
   message << ":";

   std::vector<void*> backtrace(32);
   int backtraceSize;

   backtraceSize = ::backtrace(&backtrace[0], (int) backtrace.size());
   while (backtraceSize == (int) backtrace.size())
   {
      backtrace.resize(backtraceSize * 2);
      backtraceSize = ::backtrace(&backtrace[0], (int) backtrace.size());
   }

   char** symbols = backtrace_symbols(&backtrace[0], backtraceSize);
   for (int i = 0; i < backtraceSize; i++)
      message << '\n' << i << ":\t" << symbols[i];

   LOG(GENERAL,ERR, message.str());

   exit(1);
}

}

#endif
