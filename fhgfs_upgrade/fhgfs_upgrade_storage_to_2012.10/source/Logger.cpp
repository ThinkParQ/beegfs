#include "StringTk.h"
#include "TimeAbs.h"
#include "Logger.h"

#include <time.h>

#include "ICommonConfig.h"

#define LOGGER_ROTATED_FILE_SUFFIX  ".old-"
#define LOGGER_TIMESTR_SIZE         32

#define MAX_LOG_LINE (1024) // max number of bytes per log message


Logger::Logger(ICommonConfig* cfg)
{
   this->logLevel = cfg->getLogLevel();
   this->logErrsToStdlog = cfg->getLogErrsToStdlog();
   this->logNoDate = cfg->getLogNoDate();
   this->logStdFile = cfg->getLogStdFile();
   this->logErrFile = cfg->getLogErrFile();
   this->logNumLines = cfg->getLogNumLines();
   this->logNumRotatedFiles = cfg->getLogNumRotatedFiles();
   
   this->stdFile = stdout;
   this->errFile = stderr;
   
   this->currentNumStdLines = 0;
   this->currentNumErrLines = 0;
   
   this->timeFormat = logNoDate ? "%X" : "%b%d %X"; // set time format
   
   this->rotatedFileSuffix = LOGGER_ROTATED_FILE_SUFFIX;
   
   pthread_mutex_init(&outputMutex, NULL);
   
   // prepare file handles and rotate
   prepareLogFiles();
}

Logger::~Logger()
{
   // close files
   if(this->stdFile != stdout)
      fclose(this->stdFile);
      
   if(!this->logErrsToStdlog && (this->errFile != stderr) )
      fclose(this->errFile);
      
   pthread_mutex_destroy(&outputMutex);
}

/**
 * Prints a message to the standard log.
 * Note: Doesn't lock the outputMutex
 * 
 * @param level the level of relevance (should be greater than 0)
 * @param msg the actual log message
 * @param noEOL do not print an end of line and also not the logContext
 *
 */
void Logger::logGrantedUnlocked(int level, const char* threadName, const char* context,
   const char* msg, bool noEOL)
{
   char* logBuf = (char*) malloc(MAX_LOG_LINE);

   if (noEOL)
      snprintf(logBuf, MAX_LOG_LINE - 2, "%s", msg);
   else
      snprintf(logBuf, MAX_LOG_LINE - 2, "(%d) %s", level, msg);

   if (noEOL)
      fprintf(stdFile, "%s", logBuf);
   else
      fprintf(stdFile, "%s\n", logBuf);

   if (level < Log_NOTICE)
   {
      std::cerr << logBuf << std::endl;
      std::cerr.flush();
   }

   //fflush(stdFile); // no longer needed => line buf
   
   currentNumStdLines++;
   // we don't want to rotate for the upgrade tool
   // rotateStdLogChecked();

   free(logBuf);
}

/**
 * Wrapper for logGrantedUnlocked that locks/unlocks the outputMutex.
 *
 * @param noEOL do not print an end of line and also not the logContext
 *
 */
void Logger::logGranted(int level, const char* threadName, const char* context, const char* msg,
   bool noEOL)
{
   pthread_mutex_lock(&outputMutex);
   
   logGrantedUnlocked(level, threadName, context, msg, noEOL);
   
   pthread_mutex_unlock(&outputMutex);
}

/**
 * Prints a message to the error log.
 * 
 * @param msg the actual log message
 */
void Logger::logErrGranted(const char* threadName, const char* context, const char* msg)
{
   logGranted(Log_ERR, threadName, context, msg);
}

/**
 * Prints a backtrage to the standard log.
 */ 
void Logger::logBacktraceGranted(const char* context, int backtraceLength, char** backtraceSymbols)
{
   pthread_mutex_lock(&outputMutex);
   
   logGrantedUnlocked(1, "", context, "Backtrace:");
   
   for(int i=0; i < backtraceLength; i++)
   {
      fprintf(stdFile, "%d: %s\n", i+1, backtraceSymbols[i] );
   }
   
   pthread_mutex_unlock(&outputMutex);
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
   if(!logStdFile.length() )
      stdFile = stdout;
   else
   {
      rotateLogFile(logStdFile);
      
      stdFile = fopen(logStdFile.c_str(), "w");
      if(!stdFile)
      {
         perror("Logger::openStdLogFile");
         stdFile = stdout;
         std::cerr << "Unable to create standard log file: " << logStdFile;
         ::exit(1);
      }
      
   }

   setlinebuf(stdFile);

   if(logErrsToStdlog)
      return;
   
      
   if(!logErrFile.length() )
      errFile = stderr;
   else
   {
      rotateLogFile(logErrFile);
      
      errFile = fopen(logErrFile.c_str(), "w");
      if(!errFile)
      {
         perror("Logger::openErrLogFile");
         errFile = stderr;
         std::cerr << "Unable to create error log file: " << logErrFile;
      }
      
   }

   setlinebuf(errFile);
   
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
   if((stdFile == stdout) || !logNumLines || (currentNumStdLines < logNumLines) )
      return; // nothing to do yet
      
   currentNumStdLines = 0;

   fclose(stdFile);
      
   rotateLogFile(logStdFile); // the actual rotation

   stdFile = fopen(logStdFile.c_str(), "w");
   if(!stdFile)
   {
      perror("Logger::rotateStdLogChecked");
      stdFile = stdout;
   }

   setlinebuf(stdFile);
}

void Logger::rotateErrLogChecked()
{
   if( (errFile == stderr) || !logNumLines || (currentNumErrLines < logNumLines) )
      return; // nothing to do yet
      
   currentNumErrLines = 0;
      
   fclose(errFile);

   rotateLogFile(logErrFile); // the actual rotation

   errFile = fopen(logErrFile.c_str(), "w");
   if(!errFile)
   {
      perror("Logger::rotateErrLogChecked");
      errFile = stderr;
   }

   setlinebuf(errFile);
}
