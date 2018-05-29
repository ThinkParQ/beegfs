#ifndef LOGGER_H_
#define LOGGER_H_

#include <common/app/config/ICommonConfig.h>
#include <common/app/config/InvalidConfigException.h>
#include <common/storage/StorageErrors.h>
#include <common/threading/Atomics.h>
#include <common/threading/PThread.h>
#include <common/Common.h>
#include <common/toolkit/StringTk.h>

enum LogLevel
{
   Log_ERR=0, /* system error */
   Log_CRITICAL=1, /* something the users should definitely know about */
   Log_WARNING=2, /* things that indicate or are related to a problem */
   Log_NOTICE=3, /* things that could help finding problems */
   Log_DEBUG=4, /* things that are only useful during debugging, often logged with LOG_DEBUG() */
   Log_SPAM=5, /* things that are typically too detailed even during normal debugging,
      very often with LOG_DEBUG() */
   Log_LEVELMAX /* this needs to be the last entry; it's used as array length */ 
};

enum LogTopic
{
   LogTopic_GENERAL=0,       // default log topic
   LogTopic_STATES=1,        // everything related target/node states
   LogTopic_MIRRORING=2,     // everything related to mirroring
   LogTopic_WORKQUEUES=3,    // En/dequeueing of work items
   LogTopic_STORAGEPOOLS=4,  // related to storage pools
   LogTopic_CAPACITY=5,      // capacity (pool) information
   LogTopic_COMMUNICATION=6,
   LogTopic_QUOTA=7,
   LogTopic_SESSIONS=8,      // related to session(store) handling
   LogTopic_EVENTLOGGER=9,   // related to file event logger
   LogTopic_DATABASE=10,     // related to database operations
   LogTopic_INVALID
};

class LogMessageBuilder
{
   public:
      LogMessageBuilder(const char* message, const char* extraInfoNames)
         : extraInfoNames(extraInfoNames), infosAdded(0)
      {
         buffer << message;
      }

      // this overload exists only for compatibility with old-style LOG_DEBUG
      LogMessageBuilder(const std::string& message, const char* extraInfoNames)
         : extraInfoNames(extraInfoNames), infosAdded(0)
      {
         buffer << message;
      }

      std::string finish() const
      {
         return buffer.str();
      }

   private:
      std::ostringstream buffer;
      const char* extraInfoNames;
      unsigned infosAdded;

      bool appendNextExtraInfoName(const char* suppliedName);

      template<typename T>
      LogMessageBuilder& appendNextExtraInfo(const T& value, const char* suppliedName = NULL)
      {
         if (infosAdded == 0)
            buffer << " ";
         else
            buffer << "; ";

         if (appendNextExtraInfoName(suppliedName))
            buffer << ": ";

         buffer << value;
         infosAdded++;
         return *this;
      }

   public:
      LogMessageBuilder& operator,(bool value)
      {
         return appendNextExtraInfo(value ? "yes" : "no");
      }

      // handle uint8_t differently, because ostreams write u8 out like char, not like int
      LogMessageBuilder& operator,(uint8_t value)
      {
         return appendNextExtraInfo(uint32_t(value));
      }

      // handle std::vector as comma-seperated list
      template<typename T>
      LogMessageBuilder& operator,(const std::vector<T>& value)
      {
         std::string vecAsStr = StringTk::implode(",", value);

         return appendNextExtraInfo(vecAsStr);
      }

      template<typename T>
      LogMessageBuilder& operator,(const T& value)
      {
         return appendNextExtraInfo(value);
      }

      template<typename T>
      struct RenamedExtraInfo
      {
         const char* name;
         const T* value;
      };

      struct RenameExtraInfo
      {
         template<typename T>
         RenamedExtraInfo<T> operator()(const char* name, const T& value) const
         {
            RenamedExtraInfo<T> result = {name, &value};
            return result;
         }
      };

      template<typename T>
      LogMessageBuilder& operator,(RenamedExtraInfo<T> value)
      {
         return appendNextExtraInfo(*value.value, value.name);
      }

      template<std::ios_base& (&Manip)(std::ios_base&)>
      struct InBase
      {
         template<typename T>
         std::string operator()(const T& value)
         {
            std::ostringstream out;

            out << Manip << value;
            return out.str();
         }
      };

      struct SystemError
      {
         struct Value { int num; };

         Value operator()() const
         {
            Value val = {errno};
            return val;
         }

         Value operator()(int err) const
         {
            Value val = {err};
            return val;
         }
      };

      LogMessageBuilder& operator,(const SystemError::Value& desc)
      {
         char errStrBuffer[256];
         char* errStr = strerror_r(desc.num, errStrBuffer, sizeof(errStrBuffer));

         appendNextExtraInfo(errStr, "sysErr");
         buffer << " (" << desc.num << ")";
         return *this;
      }

      LogMessageBuilder& operator,(FhgfsOpsErr error)
      {
         appendNextExtraInfo(FhgfsOpsErrTk::toErrString(error));
         buffer << " (" << int(error) << ")";
         return *this;
      }
};


class Logger
{
   private:
      struct LogTopicElem
      {
         const char* name;
         LogTopic logTopic;
      };

      static const LogTopicElem LogTopics[];

   private:
      Logger(int defaultLevel, LogType  cfgLogType, bool errsToStdlog, bool noDate, 
         const std::string& stdFile, const std::string& errFile, unsigned linesPerFile, 
         unsigned rotatedFiles);

   public:
      ~Logger();

   private:
      static std::unique_ptr<Logger> logger;

      // configurables
      LogType   logType;
      IntVector logLevels;
      bool logErrsToStdlog;
      bool logNoDate;
      std::string logStdFile;
      std::string logErrFile;
      unsigned logNumLines;
      unsigned logNumRotatedFiles;

      // internals
      pthread_rwlock_t rwLock; // cannot use RWLock class here, because that uses logging!

      const char* timeFormat;
      FILE* stdFile;
      FILE* errFile;
      AtomicUInt32 currentNumStdLines;
      AtomicUInt64 currentNumErrLines;
      std::string rotatedFileSuffix;

      void logGrantedUnlocked(int level, const char* threadName, const char* context,
         int line, const char* msg);
      void logGranted(int level, const char* threadName, const char* context, int line,
         const char* msg);
      void logErrGranted(const char* threadName, const char* context, const char* msg);
      void logBacktraceGranted(const char* context, int backtraceLength, char** backtraceSymbols);

      void prepareLogFiles();
      size_t getTimeStr(uint64_t seconds, char* buf, size_t bufLen);
      void rotateLogFile(std::string filename);
      void rotateStdLogChecked();
      void rotateErrLogChecked();

   public:
      // inliners

      void log(LogTopic logTopic, int level, const char* context, int line, const char* msg)
      {
         if(level > logLevels[logTopic])
            return;

         std::string threadName = PThread::getCurrentThreadName();

         logGranted(level, threadName.c_str(), context, line, msg);
      }

      void log(LogTopic logTopic, int level, const char* context, const char* msg)
      {
         log(logTopic, level, context, -1, msg);
      }

      /**
       * Just a wrapper for the normal log() method which takes "const char*" arguments.
       */
      void log(LogTopic logTopic, int level, const std::string& context, const std::string& msg)
      {
         if(level > logLevels[logTopic])
            return;

         log(logTopic, level, context.c_str(), msg.c_str() );
      }

      void log(int level, const char* context, const char* msg)
      {
         log(LogTopic_GENERAL, level, context, msg);
      }

      void log(int level, const std::string& context, const std::string& msg)
      {
         log(level, context.c_str(), msg.c_str());
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
         logGranted(level, threadName, context, -1, msg);
      }

      /**
       * The normal error log method.
       */
      void logErr(const char* context, const char* msg)
      {
         if(this->logErrsToStdlog)
         {
            // cppcheck-suppress nullPointer [special comment to mute false cppcheck alarm]
            log(0, context, msg);
            return;
         }

         std::string threadName = PThread::getCurrentThreadName();

         logErrGranted(threadName.c_str(), context, msg);
      }

      /**
       * Just a wrapper for the normal logErr() method which takes "const char*" arguments.
       */
      void logErr(const std::string& context, const std::string& msg)
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
      void setLogLevel(int logLevel, LogTopic logTopic = LogTopic_GENERAL)
      {
         if ((size_t)logTopic < logLevels.size())
            logLevels.at(logTopic) = logLevel;
      }

      /**
       * Note: This method is not thread-safe.
       */
      int getLogLevel(LogTopic logTopic)
      {
         try
         {
            return logLevels.at(logTopic);
         }
         catch (const std::out_of_range& e)
         {
            return -1;
         }
      }

      /**
        * Note: This method is not thread-safe.
        */
      const IntVector& getLogLevels() const
      {
         return logLevels;
      }

      static LogTopic logTopicFromName(std::string& name)
      {
         for(int i=0; LogTopics[i].logTopic != LogTopic_INVALID; i++)
         {
            if (name == LogTopics[i].name)
            {
               return LogTopics[i].logTopic;
            }
         }

         return LogTopic_INVALID;
      }

      static std::string logTopicToName(LogTopic logTopic)
      {
         for(int i=0; LogTopics[i].logTopic != LogTopic_INVALID; i++)
         {
            if (LogTopics[i].logTopic == logTopic)
            {
               return LogTopics[i].name;
            }
         }

         return LogTopics[LogTopic_INVALID].name;
      }

      static Logger* createLogger(int defaultLevel, LogType logType,  bool errsToStdlog, 
         bool noDate, const std::string& stdFile, const std::string& errFile, unsigned linesPerFile,
         unsigned rotatedFiles)
      {
         if (logger)
            throw std::runtime_error("attempted to create a second system-wide logger");

         logger.reset(new Logger(defaultLevel, logType, errsToStdlog, noDate, stdFile, errFile,
                                 linesPerFile, rotatedFiles));

         return logger.get();
      }

      static void destroyLogger()
      {
         logger.reset();
      }

      static Logger* getLogger()
      {
         return logger.get();
      }

      static bool isInitialized()
      {
         return !!logger;
      }
};

#define LOG_CTX_TOP_L(Topic, Level, Context, Line, Message, ...) \
   do { \
      Logger* const _log_logger = Logger::getLogger(); \
      if (!_log_logger || _log_logger->getLogLevel(Topic) < Level) \
         break; \
      LogMessageBuilder _log_builder(Message, #__VA_ARGS__); \
      LogMessageBuilder::RenameExtraInfo as; (void) as; \
      LogMessageBuilder::SystemError sysErr; (void) sysErr; \
      LogMessageBuilder::InBase<std::hex> hex; (void) hex; \
      LogMessageBuilder::InBase<std::oct> oct; (void) oct; \
      (void) (_log_builder, ## __VA_ARGS__); \
      _log_logger->log(Topic, Level, Context, Line, _log_builder.finish().c_str()); \
   } while (0)

#define LOG(Topic, Level, Message, ...) \
   LOG_CTX_TOP_L(LogTopic_##Topic, Log_##Level, __FILE__, __LINE__, Message, ## __VA_ARGS__)

#define LOG_CTX(Topic, Level, Context, Message, ...) \
   LOG_CTX_TOP_L(LogTopic_##Topic, Log_##Level, Context, -1, Message, ## __VA_ARGS__)

#ifdef BEEGFS_DEBUG
   #define LOG_DEBUG_CTX LOG_CTX
   // Context may be a std::string, a C string, or a string literal.
   // &X[0] turns both into a const char*
   #define LOG_DEBUG(Context, Level, Message) LOG_CTX_TOP_L(LogTopic_GENERAL, LogLevel(Level), \
                     &Context[0], -1, Message)
   #define LOG_DBG LOG
   #define LOG_TOP_DBG LOG_TOP
   #define LOG_CTX_TOP_DBG LOG_CTX_TOP
#else
   #define LOG_DEBUG_CTX(...) do {} while (0)
   #define LOG_DEBUG(...) do {} while (0)
   #define LOG_DBG(...) do {} while (0)
   #define LOG_TOP_DBG(...) do {} while (0)
   #define LOG_CTX_TOP_DBG(...) do {} while (0)
#endif

#endif /*LOGGER_H_*/
