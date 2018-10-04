#ifndef LOGCONTEXT_H_
#define LOGCONTEXT_H_

#include <common/app/log/Logger.h>
#include <common/app/AbstractApp.h>
#include <common/threading/PThread.h>
#include <common/Common.h>

#include <execinfo.h>


#define LOGCONTEXT_BACKTRACE_ARRAY_SIZE   32

#ifdef LOG_DEBUG_MESSAGES

   #define LOG_DEBUG_CONTEXT(logContext, level, msgStr) \
      do { (logContext).log(level, msgStr); } while(0)

   #define LOG_DEBUG_BACKTRACE() \
      do { LogContext(__func__).logBacktrace(); } while(0)

#else

   #define LOG_DEBUG_CONTEXT(logContext, level, msgStr) \
      do { /* nothing */ } while(0)

   #define LOG_DEBUG_BACKTRACE() \
      do { /* nothing */ } while(0)

#endif // LOG_DEBUG_MESSAGES


class LogContext
{
   public:
      LogContext(const std::string& contextStr="<undefined>") : contextStr(contextStr)
      {
         this->logger = Logger::getLogger();
      }

      void log(LogTopic logTopic, int level, const char* msg)
      {
         if(unlikely(!logger) )
            return;

         logger->log(logTopic, level, contextStr.c_str(), msg);
      }

      void log(LogTopic logTopic, int level, const std::string msg)
      {
         log(logTopic, level, msg.c_str() );
      }

      void log(int level, const char* msg)
      {
         log(LogTopic_GENERAL, level, msg);
      }

      void log(int level, const std::string& msg)
      {
         log(level, msg.c_str());
      }

      void logErr(const char* msg)
      {
         if(unlikely(!logger) )
            return;

         logger->log(Log_ERR, contextStr.c_str(), msg);
      }

      void logErr(const std::string& msg)
      {
         logErr(msg.c_str() );
      }

      void logBacktrace()
      {
         int backtraceLength = 0;
         char** backtraceSymbols = NULL;

         void* backtraceArray[LOGCONTEXT_BACKTRACE_ARRAY_SIZE];
         backtraceLength = backtrace(backtraceArray, LOGCONTEXT_BACKTRACE_ARRAY_SIZE);

         // note: symbols are malloc'ed and need to be freed later
         backtraceSymbols = backtrace_symbols(backtraceArray, backtraceLength);

         if(unlikely(!logger) )
            return;

         logger->logBacktrace(contextStr.c_str(), backtraceLength, backtraceSymbols);

         SAFE_FREE(backtraceSymbols);
      }


   protected:
      LogContext(Logger* logger, const std::string& context) : contextStr(context)
      {
         // for derived classes that have a different way of finding the logger
         // (other than via the thread-local storage)
         // ...and remember to init the context also ;)

         this->logger = logger;
      }


      // getters & setters
      void setLogger(Logger* logger)
      {
         this->logger = logger;
      }


   private:
      const std::string contextStr;
      Logger* logger;

};

#endif /*LOGCONTEXT_H_*/
