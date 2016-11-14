#ifndef LOGGER_H_
#define LOGGER_H_

#include <common/app/config/ICommonConfig.h>
#include <common/app/config/InvalidConfigException.h>
#include <common/threading/PThread.h>
#include <common/Common.h>

enum LogLevel
{
   Log_ERR=0, /* system error */
   Log_CRITICAL=1, /* something the users should definitely know about */
   Log_WARNING=2, /* things that indicate or are related to a problem */
   Log_NOTICE=3, /* things that could help finding problems */
   Log_DEBUG=4, /* things that are only useful during debugging, normally logged with LOG_DEBUG() */
   Log_SPAM=5 /* things that are typically too detailed even during normal debugging,
      definitely with LOG_DEBUG only */
};

class Logger
{
   public:
      Logger(ICommonConfig* cfg) throw(InvalidConfigException);
      ~Logger();
   
   private:
      // configurables
      int logLevel;
      bool logErrsToStdlog;
      bool logNoDate;
      std::string logStdFile;
      std::string logErrFile;
      unsigned logNumLines;
      unsigned logNumRotatedFiles;

      // internals
      pthread_mutex_t outputMutex; // cannot use the Mutex class here, because that uses logging!
      
      const char* timeFormat;
      FILE* stdFile;
      FILE* errFile;
      unsigned currentNumStdLines;
      unsigned currentNumErrLines;
      std::string rotatedFileSuffix;

      void logGrantedUnlocked(int level, const char* threadName, const char* context,
         const char* msg, bool noEOL = false);
      void logGranted(int level, const char* threadName, const char* context, const char* msg,
         bool noEOL = false);
      void logErrGranted(const char* threadName, const char* context, const char* msg);
      void logBacktraceGranted(const char* context, int backtraceLength, char** backtraceSymbols);
      
      void prepareLogFiles() throw(InvalidConfigException);
      size_t getTimeStr(uint64_t seconds, char* buf, size_t bufLen);
      void rotateLogFile(std::string filename);
      void rotateStdLogChecked();
      void rotateErrLogChecked();
      
   public:
      // inliners
      
      /**
       * The normal log method.
       *
       * @param level Log_...
       * @param noEOL do not print an end of line and also not the logContext
       */
      void log(int level, const char* context, const char* msg, bool noEOL = false)
      {
         if(level > this->logLevel)
            return;
            
         std::string threadName = PThread::getCurrentThreadName();

         logGranted(level, threadName.c_str(), context, msg, noEOL);
      }

      /**
       * Just a wrapper for the normal log() method which takes "const char*" arguments.
       * @param noEOL do not print an end of line and also not the logContext
       */
      void log(int level, const std::string context, const std::string msg, bool noEOL = false)
      {
         if(level > this->logLevel)
            return;

         log(level, context.c_str(), msg.c_str(), noEOL);
      }

      /**
       * Special log method that allows logging independent of the configured log level.
       *
       * Note: Used by helperd to log client messages independent of helperd log level.
       *
       * @param forceLogging true to force logging without checking the log level.
       */
      void logForcedWithThreadName(int level, const char* threadName, const char* context,
         const char* msg)
      {
         logGranted(level, threadName, context, msg);
      }
      
      /**
       * The normal error log method.
       */
      void logErr(const char* context, const char* msg)
      {
         if(this->logErrsToStdlog)
         {
            log(0, context, msg);
            return;
         }
         
         std::string threadName = PThread::getCurrentThreadName();
         
         logErrGranted(threadName.c_str(), context, msg);
      }

      /**
       * Just a wrapper for the normal logErr() method which takes "const char*" arguments.
       */
      void logErr(const std::string context, const std::string msg)
      {
         logErr(context.c_str(), msg.c_str() );
      }

      
      /**
       * Special version with addidional threadName argument.
       *
       * Note: Used by helperd to log client messages with threadName given by client.
       */
      void logErrWithThreadName(const char* threadName, const char* context, const char* msg)
      {
         if(this->logErrsToStdlog)
         {
            logForcedWithThreadName(0, threadName, context, msg);
            return;
         }

         logErrGranted(threadName, context, msg);
      }

      void logBacktrace(const char* context, int backtraceLength, char** backtraceSymbols)
      {
         if(!backtraceSymbols)
         {
            log(1, context, "Note: No symbols for backtrace available");
            return;
         }

         logBacktraceGranted(context, backtraceLength, backtraceSymbols);
      }
      
      // getters & setters
      
      /**
       * Note: This method is not thread-safe.
       */
      void setLogLevel(int logLevel)
      {
         this->logLevel = logLevel;
      }
      
      /**
       * Note: This method is not thread-safe.
       */
      int getLogLevel()
      {
         return this->logLevel;
      }
};

#endif /*LOGGER_H_*/
