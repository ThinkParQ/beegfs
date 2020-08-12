#ifndef LOGGER_H_
#define LOGGER_H_

#include <app/config/Config.h>
#include <app/App.h>
#include <common/Common.h>
#include <common/toolkit/StringTk.h>
#include <common/toolkit/Time.h>
#include <common/threading/Mutex.h>
#include <common/Common.h>
#include <common/Common.h>
#include <toolkit/ExternalHelperd.h>


#define LOGGER_LOGBUF_SIZE          1000 /* max log message length */
#define LOGGER_PID_NOCURRENTOUTPUT     0 /* pid value if outputMutex not locked */


#ifdef LOG_DEBUG_MESSAGES

   #define LOG_DEBUG(logger, level, contextStr, msgStr) \
      do { Logger_log(logger, level, contextStr, msgStr); } while(0)

   #define LOG_DEBUG_TOP(logger, logTopic, level, contextStr, msgStr) \
      do { Logger_logTop(logger, logTopic, level, contextStr, msgStr); } while(0)

   #define LOG_DEBUG_FORMATTED(logger, level, contextStr, msgStr, ...) \
      do { Logger_logFormatted(logger, level, contextStr, msgStr, ## __VA_ARGS__); } while(0)

   #define LOG_DEBUG_TOP_FORMATTED(logger, logTopic, level, contextStr, msgStr, ...) \
      do { Logger_logTopFormatted(logger, logTopic, level, contextStr, msgStr, ## __VA_ARGS__); } \
         while(0)

#else

   #define LOG_DEBUG(logger, level, contextStr, msgStr)
   #define LOG_DEBUG_TOP(logger, logTopic, level, contextStr, msgStr)
   #define LOG_DEBUG_FORMATTED(logger, level, contextStr, msgStr, ...)
   #define LOG_DEBUG_TOP_FORMATTED(logger, logTopic, level, contextStr, msgStr, ...)

#endif // LOG_DEBUG_MESSAGES

#define Logger_logFormattedWithEntryID(inode, level, logContext, msgFormat, ...) \
   Logger_logTopFormattedWithEntryID(inode, LogTopic_GENERAL, level, logContext, msgFormat, \
      ##__VA_ARGS__)

// forward declarations...

struct Logger;
typedef struct Logger Logger;

struct Node;

enum LogLevel;
typedef enum LogLevel LogLevel;
enum LogTopic;
typedef enum LogTopic LogTopic;



extern void Logger_init(Logger* this, App* app, Config* cfg);
extern Logger* Logger_construct(App* app, Config* cfg);
extern void Logger_uninit(Logger* this);
extern void Logger_destruct(Logger* this);

__attribute__((format(printf, 4, 5)))
extern void Logger_logFormatted(Logger* this, LogLevel level, const char* context,
   const char* msgFormat, ...);
__attribute__((format(printf, 5, 6)))
extern void Logger_logTopFormattedWithEntryID(struct inode* inode, LogTopic logTopic,
   LogLevel level, const char* logContext, const char* msgFormat, ...);
__attribute__((format(printf, 5, 6)))
extern void Logger_logTopFormatted(Logger* this, LogTopic logTopic, LogLevel level,
   const char* context, const char* msgFormat, ...);
extern void Logger_logFormattedVA(Logger* this, LogLevel level, const char* context,
   const char* msgFormat, va_list ap);
extern void Logger_logTopFormattedVA(Logger* this, LogTopic logTopic, LogLevel level,
   const char* context, const char* msgFormat, va_list ap);
__attribute__((format(printf, 3, 4)))
extern void Logger_logErrFormatted(Logger* this, const char* context, const char* msgFormat, ...);
__attribute__((format(printf, 4, 5)))
extern void Logger_logTopErrFormatted(Logger* this, LogTopic logTopic, const char* context,
   const char* msgFormat, ...);

extern LogLevel Logger_getLogLevel(Logger* this);
extern LogLevel Logger_getLogTopicLevel(Logger* this, LogTopic logTopic);

extern void __Logger_logTopGrantedUnlocked( Logger* this, LogTopic logTopic, LogLevel level,
   const char* context, const char* msg);
extern void __Logger_logTopFormattedGranted(Logger* this, LogTopic logTopic, LogLevel level,
   const char* context, const char* msgFormat, va_list args);

extern bool __Logger_checkThreadMultiLock(Logger* this);
extern void __Logger_setCurrentOutputPID(Logger* this, pid_t pid);
extern void __Logger_logReachableLoggerd(Logger* this);
extern void __Logger_logUnreachableLoggerd(Logger* this);


// static
extern const char* Logger_getLogTopicStr(LogTopic logTopic);
extern bool Logger_getLogTopicFromStr(const char* logTopicStr, LogTopic* logTopic);

// getters & setters
static inline void Logger_setClientID(Logger* this, const char* clientID);
static inline void Logger_setAllLogLevels(Logger* this, LogLevel logLevel);
static inline void Logger_setLogTopicLevel(Logger* this, LogTopic logTopic, LogLevel logLevel);

// inliners
static inline void Logger_log(Logger* this, LogLevel level, const char* context, const char* msg);
static inline void Logger_logTop(Logger* this, LogTopic logTopic, LogLevel level,
   const char* context, const char* msg);
static inline void Logger_logErr(Logger* this, const char* context, const char* msg);
static inline void Logger_logTopErr(Logger* this, LogTopic logTopic, const char* context,
   const char* msg);


enum LogLevel
{
   LOG_NOTHING=-1,
   Log_ERR=0, /* system error */
   Log_CRITICAL=1, /* something the users should definitely know about */
   Log_WARNING=2, /* things that indicate or are related to a problem */
   Log_NOTICE=3, /* things that could help finding problems */
   Log_DEBUG=4, /* things that are only useful during debugging, often logged with LOG_DEBUG() */
   Log_SPAM=5 /* things that are typically too detailed even during normal debugging,
      very often with LOG_DEBUG() */
};

/**
 * Note: When you add a new log topic, you must also update these places:
 *  1) Logger_getLogTopicStr()
 *  2) ProcFsHelper_{read/write}_logLevels()
 */
enum LogTopic
{
   LogTopic_GENERAL=0, /* everything that is not assigned to a more specific log topic */
   LogTopic_CONN,      /* connects and disconnects */
   LogTopic_COMMKIT,   /* CommKitVec */

   LogTopic_LAST      /* not valid, just exists to define the LogLevelsArray size */
};

typedef signed char LogTopicLevels[LogTopic_LAST]; /* array for per-topic log levels, see LogTopic/
   LogLevel. Note: Type is actually type LogLevel, but we use char here because we also allow
   "-1" to disable a level. */


/**
 *  This is the general logger class. It can to log to helperd or to syslog.
 */
struct Logger
{
   // configurables
   LogTopicLevels logLevels; // per-topic log levels

   // internals
   App* app;

   Mutex outputMutex;

   Mutex multiLockMutex; // to avoid multiple locking of the outputMutex by the same thread
   pid_t currentOutputPID; // pid of outputMutex holder (see LOGGER_PID_NOCURRENTOUTPUT)

   struct Node* loggerdNode; // can be NULL (e.g. during app destruction)
   bool loggerdReachable;

   char* logFormattedBuf; // for logging functions with variable argument list
   char* logContextBuf; // for extended context logging

   char* clientID; // only set if clientID logging is enabled
};


/**
 * Note: Copies the clientID.
 */
void Logger_setClientID(Logger* this, const char* clientID)
{
   SAFE_KFREE(this->clientID); // free old clientID

   this->clientID = StringTk_strDup(clientID);
}

/**
 * Note: This is intended to be used during app destruction to disable logging by setting the levels
 * to "-1".
 *
 * @param logLevel LogLevel_... or "-1" to disable.
 */
void Logger_setAllLogLevels(Logger* this, LogLevel logLevel)
{
   int i;

   for(i=0; i < LogTopic_LAST; i++)
      this->logLevels[i] = logLevel;
}

/**
 * @param logLevel LogLevel_... or "-1" to disable.
 */
void Logger_setLogTopicLevel(Logger* this, LogTopic logTopic, LogLevel logLevel)
{
   this->logLevels[logTopic] = logLevel;
}

/**
 * Log msg for LogTopic_GENERAL.
 *
 * @param level LogLevel_... value
 * @param context the context from which this msg was printed (e.g. the calling function).
 * @param msg the log message
 */
void Logger_log(Logger* this, LogLevel level, const char* context, const char* msg)
{
   Logger_logFormatted(this, level, context, "%s", msg);
}

/**
 * Log msg for a certain log topic.
 *
 * @param level LogLevel_... value
 * @param context the context from which this msg was printed (e.g. the calling function).
 * @param msg the log message
 */
void Logger_logTop(Logger* this, LogTopic logTopic, LogLevel level, const char* context,
   const char* msg)
{
   Logger_logTopFormatted(this, logTopic, level, context, "%s", msg);
}

/**
 * Log error msg for LogTopic_GENERAL.
 */
void Logger_logErr(Logger* this, const char* context, const char* msg)
{
   Logger_logErrFormatted(this, context, "%s", msg);
}

/**
 * Log error msg for a certain log topic.
 */
void Logger_logTopErr(Logger* this, LogTopic logTopic, const char* context, const char* msg)
{
   Logger_logTopErrFormatted(this, logTopic, context, "%s", msg);
}






#endif /*LOGGER_H_*/
