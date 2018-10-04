#include <common/toolkit/StringTk.h>
#include <common/threading/PThread.h>
#include <common/toolkit/TimeAbs.h>
#include "Logger.h"

#include <ctime>

#undef  LOG_DEBUG
#include <syslog.h>

#define LOGGER_ROTATED_FILE_SUFFIX  ".old-"
#define LOGGER_TIMESTR_SIZE         32

std::unique_ptr<Logger> Logger::logger;

// Note: Keep in sync with enum LogTopic
const char* const Logger::LogTopics[LogTopic_INVALID] =
{
   "general",
   "states",
   "mirroring",
   "workqueues",
   "storage-pools",
   "capacity",
   "communication",
   "quota",
   "sessions",
   "event-logger",
   "database",
   "socklib",
};

static const int syslogLevelMapping[Log_LEVELMAX] = { LOG_ERR, LOG_CRIT, LOG_WARNING, 
   LOG_NOTICE, LOG_DEBUG, LOG_DEBUG };

Logger::Logger(int defaultLevel, LogType cfgLogType,  bool noDate, const std::string& stdFile,
      unsigned linesPerFile, unsigned rotatedFiles):
   logType(cfgLogType), logLevels(LogTopic_INVALID, defaultLevel),
   logNoDate(noDate),logStdFile(stdFile), logNumLines(linesPerFile),logNumRotatedFiles(rotatedFiles)
{
   this->stdFile = stdout;
   this->errFile = stderr;

   this->timeFormat = logNoDate ? "%X" : "%b%d %X"; // set time format

   this->rotatedFileSuffix = LOGGER_ROTATED_FILE_SUFFIX;

   pthread_rwlockattr_t attr;
   pthread_rwlockattr_init(&attr);
   pthread_rwlockattr_setkind_np(&attr, PTHREAD_RWLOCK_PREFER_READER_NP);

   pthread_rwlock_init(&this->rwLock, &attr);

   // prepare file handles and rotate
   prepareLogFiles();
}

Logger::~Logger()
{
   // close files
   if(this->stdFile != stdout)
      fclose(this->stdFile);

   pthread_rwlock_destroy(&this->rwLock);
}

/**
 * Prints a message to the standard log.
 * Note: Doesn't lock the outputLock
 *
 * @param level the level of relevance (should be greater than 0)
 * @param msg the actual log message
 */
void Logger::logGrantedUnlocked(int level, const char* threadName, const char* context,
   int line, const char* msg)
{
   char timeStr[LOGGER_TIMESTR_SIZE];

   TimeAbs nowTime;

   getTimeStr(nowTime.getTimeS(), timeStr, LOGGER_TIMESTR_SIZE);

   if (line >= 0)
   {
      const char* contextEnd = context + ::strlen(context) - 1;
      while (contextEnd > context && *contextEnd != '/')
         contextEnd--;
      if (*contextEnd == '/')
         context = contextEnd + 1;
      else
         context = contextEnd;
   }

   if ( logType != LogType_SYSLOG )
   { 
#ifdef BEEGFS_DEBUG_PROFILING
      uint64_t timeMicroS = nowTime.getTimeMicroSecPart(); // additional micro-s info for timestamp

      if (line > 0)
         fprintf(stdFile, "(%d) %s.%06ld %s [%s:%i] >> %s\n", level, timeStr, (long) timeMicroS,
               threadName, context, line, msg);
      else
         fprintf(stdFile, "(%d) %s.%06ld %s [%s] >> %s\n", level, timeStr, (long) timeMicroS,
               threadName, context, msg);
#else
      if (line > 0)
         fprintf(stdFile, "(%d) %s %s [%s:%i] >> %s\n", level, timeStr, threadName, context, line,
               msg);
      else
         fprintf(stdFile, "(%d) %s %s [%s] >> %s\n", level, timeStr, threadName, context, msg);
#endif // BEEGFS_DEBUG_PROFILING

   //fflush(stdFile); // no longer needed => line buf

      currentNumStdLines.increase();
   }
   else
   {
      if (line > 0)
         syslog(syslogLevelMapping[level], "%s [%s:%i] >> %s\n", threadName, context, line, msg);
      else
         syslog(syslogLevelMapping[level], "%s [%s] >> %s\n", threadName, context, msg);
   }
}

/**
 * Wrapper for logGrantedUnlocked that locks/unlocks the outputMutex.
 */
void Logger::logGranted(int level, const char* threadName, const char* context, int line,
   const char* msg)
{
   pthread_rwlock_rdlock(&this->rwLock);

   logGrantedUnlocked(level, threadName, context, line, msg);

   pthread_rwlock_unlock(&this->rwLock);

   rotateStdLogChecked();
}

/**
 * Prints a backtrage to the standard log.
 */
void Logger::logBacktraceGranted(const char* context, int backtraceLength, char** backtraceSymbols)
{
   std::string threadName = PThread::getCurrentThreadName();

   pthread_rwlock_rdlock(&this->rwLock);

   logGrantedUnlocked(1, threadName.c_str(), context, -1, "Backtrace:");

   for(int i=0; i < backtraceLength; i++)
   {
      fprintf(stdFile, "%d: %s\n", i+1, backtraceSymbols[i] );
   }

   pthread_rwlock_unlock(&this->rwLock);
}

/*
 * Print time and date to buf.
 *
 * @param buf output buffer
 * @param bufLen length of buf (depends on current locale, at least 32 recommended)
 * @return 0 on error, strLen of buf otherwise
 */
size_t Logger::getTimeStr(uint64_t seconds, char* buf, size_t bufLen)
{
   struct tm nowStruct;
   localtime_r((time_t*) &seconds, &nowStruct);

   size_t strRes = strftime(buf, bufLen, timeFormat, &nowStruct);
   buf[strRes] = 0; // just to make sure - in case the given buf is too small
   return strRes;
}

void Logger::prepareLogFiles()
{
   if ( logType == LogType_SYSLOG )
   {
      //if its sylog skip the file operation
      return;
   }
   else if(!logStdFile.length() )
   {
      stdFile = stdout;
   }
   else
   {
      rotateLogFile(logStdFile);

      stdFile = fopen(logStdFile.c_str(), "w");
      if(!stdFile)
      {
         perror("Logger::openStdLogFile");
         stdFile = stdout;
         throw InvalidConfigException(
            std::string("Unable to create standard log file: ") + logStdFile);
      }

   }

   setlinebuf(stdFile);

   return;
}

void Logger::rotateLogFile(std::string filename)
{
   for(int i=logNumRotatedFiles; i > 0; i--)
   {
      std::string oldname = filename +
         ( (i==1) ? "" : (rotatedFileSuffix + StringTk::intToStr(i-1) ) );

      std::string newname = filename +
         rotatedFileSuffix + StringTk::intToStr(i);

      rename(oldname.c_str(), newname.c_str() );
   }

}

void Logger::rotateStdLogChecked()
{

   if( (stdFile == stdout) || logType == LogType_SYSLOG ||
       !logNumLines  || (currentNumStdLines.read() < logNumLines) )
      return; // nothing to do yet

   pthread_rwlock_wrlock(&this->rwLock);

   if (currentNumStdLines.read() < logNumLines)
   { // we raced with another thread before aquiring the lock
      pthread_rwlock_unlock(&this->rwLock);
      return;
   }

   currentNumStdLines.setZero();

   fclose(stdFile);

   rotateLogFile(logStdFile); // the actual rotation

   stdFile = fopen(logStdFile.c_str(), "w");
   if(!stdFile)
   {
      perror("Logger::rotateStdLogChecked");
      stdFile = stdout;
   }

   setlinebuf(stdFile);

   pthread_rwlock_unlock(&this->rwLock);
}
