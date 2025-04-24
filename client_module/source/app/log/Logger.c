#include <common/threading/Thread.h>
#include <common/toolkit/MessagingTk.h>
#include <common/nodes/Node.h>
#include <filesystem/FhgfsOpsSuper.h>
#include <filesystem/FhgfsInode.h>
#include <toolkit/NoAllocBufferStore.h>
#include "Logger.h"


#define LOG_TOPIC_GENERAL_STR    "general"
#define LOG_TOPIC_CONN_STR       "conn"
#define LOG_TOPIC_COMMKIT_STR    "commkit"
#define LOG_TOPIC_UNKNOWN_STR    "<unknown>" /* for unknown/invalid log topics */



void Logger_init(Logger* this, App* app, Config* cfg)
{
   int i;

   this->app = app;

   for(i=0; i < LogTopic_LAST; i++)
      this->logLevels[i] = Config_getLogLevel(cfg);

   this->logFormattedBuf = (char*)os_kmalloc(LOGGER_LOGBUF_SIZE);
   this->logContextBuf = (char*)os_kmalloc(LOGGER_LOGBUF_SIZE);

   this->clientID = NULL;

   Mutex_init(&this->outputMutex);


   // Note: The follwing guys exist to avoid deadlocks that would occur when log messages are
   //    created (by the same thread) while we're already trying to send a log message to the
   //    helper daemon (e.g. the messages of the NodeConnPool). Such messages will be discarded.
   this->currentOutputPID = LOGGER_PID_NOCURRENTOUTPUT;
   Mutex_init(&this->multiLockMutex);
}

Logger* Logger_construct(App* app, Config* cfg)
{
   Logger* this = (Logger*)os_kmalloc(sizeof(Logger) );

   Logger_init(this, app, cfg);

   return this;
}

void Logger_uninit(Logger* this)
{
   SAFE_KFREE(this->clientID);

   SAFE_KFREE(this->logContextBuf);
   SAFE_KFREE(this->logFormattedBuf);

   Mutex_uninit(&this->multiLockMutex);
   Mutex_uninit(&this->outputMutex);
}

void Logger_destruct(Logger* this)
{
   Logger_uninit(this);

   kfree(this);
}

/**
 * Just print a log message with formatting similar to printk().
 *
 * @param level LogLevel_... value
 * @param context the context from which this msg was printed (e.g. the calling function).
 * @param msg the log message with formatting, e.g. "%s".
 */
void Logger_logFormatted(Logger* this, LogLevel level, const char* context, const char* msgFormat,
   ...)
{
   // note: cannot be inlined because of variable arg list

   va_list ap;

   if(level > this->logLevels[LogTopic_GENERAL])
      return;

   va_start(ap, msgFormat);

   __Logger_logTopFormattedGranted(this, LogTopic_GENERAL, level, context, msgFormat, ap);

   va_end(ap);
}

void Logger_logTopFormatted(Logger* this, LogTopic logTopic, LogLevel level, const char* context,
   const char* msgFormat, ...)
{
   // note: cannot be inlined because of variable arg list

   va_list ap;

   if(level > this->logLevels[logTopic])
      return;

   va_start(ap, msgFormat);

   __Logger_logTopFormattedGranted(this, logTopic, level, context, msgFormat, ap);

   va_end(ap);
}

/**
 * Log with EntryID
 *
 * Note: This takes an EntryInfo read-lock. Must be used only if there is no risk of deadlock.
 */
void Logger_logTopFormattedWithEntryID(struct inode* inode, LogTopic logTopic, LogLevel level,
   const char* logContext, const char* msgFormat, ...)
{
   char* newMsg;
   App* app = FhgfsOps_getApp(inode->i_sb);
   Logger* log = App_getLogger(app);
   FhgfsInode* fhgfsInode = BEEGFS_INODE(inode);
   const EntryInfo* entryInfo = FhgfsInode_getEntryInfo(fhgfsInode);
   va_list ap;

   FhgfsInode_entryInfoReadLock(fhgfsInode); // L O C K entryInfo

   va_start(ap, msgFormat);

   newMsg = os_kmalloc(LOGGER_LOGBUF_SIZE);
   if (newMsg)
      snprintf(newMsg, LOGGER_LOGBUF_SIZE, "entryID: %s %s ", EntryInfo_getEntryID(entryInfo),
         msgFormat);
   else // malloc failed. Likely an out memory situation, we still try to print msgFormat
      newMsg = (char*)msgFormat;

   Logger_logTopFormattedVA(log, logTopic, level, logContext, newMsg, ap);
   va_end(ap);

   FhgfsInode_entryInfoReadUnlock(fhgfsInode); // U N L O C K entryInfo

   if(newMsg != msgFormat)
      kfree(newMsg);
}




/**
 * Just print a log message. Similar to Logger_logFormatted(), but take a va_list already
 */
void Logger_logFormattedVA(Logger* this, LogLevel level, const char* context, const char* msgFormat,
   va_list ap)
{
   if(level > this->logLevels[LogTopic_GENERAL])
      return;

   __Logger_logTopFormattedGranted(this, LogTopic_GENERAL, level, context, msgFormat, ap);
}

/**
 * Just print a log message. Similar to Logger_logTopFormatted(), but take a va_list already
 */
void Logger_logTopFormattedVA(Logger* this, LogTopic logTopic, LogLevel level, const char* context,
   const char* msgFormat, va_list ap)
{
   if(level > this->logLevels[logTopic])
      return;

   __Logger_logTopFormattedGranted(this, logTopic, level, context, msgFormat, ap);
}

void Logger_logErrFormatted(Logger* this, const char* context, const char* msgFormat, ...)
{
   // note: cannot be inlined because of variable arg list

   va_list ap;

   va_start(ap, msgFormat);

   __Logger_logTopFormattedGranted(this, LogTopic_GENERAL, Log_ERR, context, msgFormat, ap);

   va_end(ap);
}

void Logger_logTopErrFormatted(Logger* this, LogTopic logTopic, const char* context,
   const char* msgFormat, ...)
{
   // note: cannot be inlined because of variable arg list

   va_list ap;

   va_start(ap, msgFormat);

   __Logger_logTopFormattedGranted(this, logTopic, Log_ERR, context, msgFormat, ap);

   va_end(ap);
}

/**
 * Prints a message to the standard log.
 *
 * @param level log level (Log_... value)
 * @param msg the message
 */
void __Logger_logTopFormattedGranted(Logger* this, LogTopic logTopic, LogLevel level,
   const char* context, const char* msgFormat, va_list args)
{
   if(__Logger_checkThreadMultiLock(this) )
   {
      // this thread is already trying to log a message. trying to lock outputMutex would deadlock.
      //    => discard this message

      return;
   }

   Mutex_lock(&this->outputMutex);

   __Logger_setCurrentOutputPID(this, current->pid); // grab currentOutputPID


   // evaluate msgFormat
   vsnprintf(this->logFormattedBuf, LOGGER_LOGBUF_SIZE, msgFormat, args);

   // extend context
   if(this->clientID)
      snprintf(this->logContextBuf, LOGGER_LOGBUF_SIZE, "%s: %s", this->clientID, context);
   else
      snprintf(this->logContextBuf, LOGGER_LOGBUF_SIZE, "%s", context);

   printk_fhgfs(KERN_INFO, "%s: %s\n", this->logContextBuf, this->logFormattedBuf);

   __Logger_setCurrentOutputPID(this, LOGGER_PID_NOCURRENTOUTPUT); // release currentOutputPID

   Mutex_unlock(&this->outputMutex);
}


/**
 * Note: Call this before locking the outputMutex (because it exists to avoid dead-locking).
 *
 * @return true if the currentOutputPID is set to the current thread and logging cannot continue;
 */
bool __Logger_checkThreadMultiLock(Logger* this)
{
   bool retVal = false;

   Mutex_lock(&this->multiLockMutex);

   if(this->currentOutputPID == current->pid)
   { // we alread own the outputPID (=> we already own the outputMutex)
      retVal = true;
   }

   Mutex_unlock(&this->multiLockMutex);

   return retVal;
}

/**
 * Note: Call this only after the thread owns the outputMutex to avoid "stealing".
 */
void __Logger_setCurrentOutputPID(Logger* this, pid_t pid)
{
   Mutex_lock(&this->multiLockMutex);

   this->currentOutputPID = pid;

   Mutex_unlock(&this->multiLockMutex);
}


/**
 * Returns a pointer to the static string representation of a log topic (or "<unknown>" for unknown/
 * invalid log topic numbers.
 */
const char* Logger_getLogTopicStr(LogTopic logTopic)
{
   switch(logTopic)
   {
      case LogTopic_GENERAL:
         return LOG_TOPIC_GENERAL_STR;

      case LogTopic_CONN:
         return LOG_TOPIC_CONN_STR;

      case LogTopic_COMMKIT:
         return LOG_TOPIC_COMMKIT_STR;

      default:
         return LOG_TOPIC_UNKNOWN_STR;
   }
}

/**
 * Returns the log topic number from a string (not case-sensitive).
 *
 * @return false if string didn't match any known log topic.
 */
bool Logger_getLogTopicFromStr(const char* logTopicStr, LogTopic* outLogTopic)
{
   int i;

   for(i=0; i < LogTopic_LAST; i++)
   {
      const char* currentLogTopicStr = Logger_getLogTopicStr( (LogTopic)i);

      if(!strcasecmp(logTopicStr, currentLogTopicStr))
      {
         *outLogTopic = i;
         return true;
      }
   }

   // (note: we carefully set outLogTopic to "general" to not risk leaving it undefined)
   *outLogTopic = LogTopic_GENERAL;
   return false;
}

/**
 * Shortcut to retrieve the level of LogTopic_GENERAL in old code.
 * New code should use _getLogTopicLevel() instead.
 */
LogLevel Logger_getLogLevel(Logger* this)
{
   return this->logLevels[LogTopic_GENERAL];
}

LogLevel Logger_getLogTopicLevel(Logger* this, LogTopic logTopic)
{
   return this->logLevels[logTopic];
}

