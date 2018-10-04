#ifndef LOGGER_H_
#define LOGGER_H_

#include <common/app/config/ICommonConfig.h>
#include <common/app/config/InvalidConfigException.h>
#include <common/storage/StorageErrors.h>
#include <common/threading/Atomics.h>
#include <common/threading/PThread.h>
#include <common/Common.h>
#include <common/toolkit/StringTk.h>

#include <boost/io/ios_state.hpp>
#include <boost/preprocessor/comparison/equal.hpp>
#include <boost/preprocessor/if.hpp>
#include <boost/preprocessor/punctuation/is_begin_parens.hpp>
#include <boost/preprocessor/seq/elem.hpp>
#include <boost/preprocessor/seq/first_n.hpp>
#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/seq/pop_front.hpp>
#include <boost/preprocessor/seq/rest_n.hpp>
#include <boost/preprocessor/seq/size.hpp>
#include <boost/preprocessor/stringize.hpp>
#include <boost/preprocessor/tuple/elem.hpp>
#include <boost/preprocessor/tuple/to_seq.hpp>

#include <system_error>

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
   LogTopic_SOCKLIB=11,      // socket library message (eg ib_lib)
   LogTopic_INVALID
};

namespace beegfs { namespace logging {

struct SystemError
{
   int value;

   SystemError() : value(errno) {}
   explicit SystemError(int value) : value(value) {}

   SystemError operator()(int e) const { return SystemError(e); }

   friend std::ostream& operator<<(std::ostream& os, SystemError e)
   {
      char errStrBuffer[256];
      char* errStr = strerror_r(e.value, errStrBuffer, sizeof(errStrBuffer));

      boost::io::ios_all_saver flags(os);

      os.flags(std::ios_base::dec);
      os.width(0);

      return os << errStr << " (" << e.value << ")";
   }
};

template<std::ios_base& (&Manip)(std::ios_base&)>
struct InBase
{
   constexpr InBase() {}

   template<typename T>
   std::string operator()(const T& value) const
   {
      std::ostringstream out;

      out << Manip << value;
      return out.str();
   }
};

struct LogInfos
{
   std::stringstream infos;

   template<typename T>
   LogInfos& operator<<(T data)
   {
      infos << data;
      return *this;
   }

   LogInfos& operator<<(bool data)
   {
      infos << (data ? "yes" : "no");
      return *this;
   }

   /*
    * std::error_code does have an operator<<, but this prints the numeric error code, not the
    * error message, so we define our own one that pretty-prints message and category.
    */
   LogInfos& operator<<(std::error_code err)
   {
      infos << err.message() << " (" << err.category().name() << ": " << err.value() << ")";
      return *this;
   }

   std::string str() const
   {
      return infos.str();
   }
};

}} // beegfs::logging


class Logger
{
   private:
      static const char* const LogTopics[LogTopic_INVALID];

   private:
      Logger(int defaultLevel, LogType cfgLogType, bool noDate, const std::string& stdFile,
            unsigned linesPerFile, unsigned rotatedFiles);

   public:
      ~Logger();

   private:
      static std::unique_ptr<Logger> logger;

      // configurables
      LogType   logType;
      IntVector logLevels;
      bool logNoDate;
      std::string logStdFile;
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
      void logBacktraceGranted(const char* context, int backtraceLength, char** backtraceSymbols);

      void prepareLogFiles();
      size_t getTimeStr(uint64_t seconds, char* buf, size_t bufLen);
      void rotateLogFile(std::string filename);
      void rotateStdLogChecked();

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
       * Special version with addidional threadName argument.
       *
       * Note: Used by helperd to log client messages with threadName given by client.
       */
      void logErrWithThreadName(const char* threadName, const char* context, const char* msg)
      {
         logForcedWithThreadName(0, threadName, context, msg);
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

      static LogTopic logTopicFromName(const std::string& name)
      {
         const auto idx = std::find_if(
               std::begin(LogTopics), std::end(LogTopics),
               [&] (const char* c) { return c == name; });

         if (idx == std::end(LogTopics))
            return LogTopic_INVALID;

         return LogTopic(idx - std::begin(LogTopics));
      }

      static std::string logTopicToName(LogTopic logTopic)
      {
         return LogTopics[logTopic];
      }

      static Logger* createLogger(int defaultLevel, LogType logType, bool noDate,
            const std::string& stdFile, unsigned linesPerFile, unsigned rotatedFiles)
      {
         if (logger)
            throw std::runtime_error("attempted to create a second system-wide logger");

         logger.reset(new Logger(defaultLevel, logType, noDate, stdFile, linesPerFile,
                  rotatedFiles));

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

#define LOG_CTX_TOP_L_ITEM__sep \
   (_log_line << (_log_items++ ? "; " : " "))
#define LOG_CTX_TOP_L_ITEM__1(item) \
   LOG_CTX_TOP_L_ITEM__sep << BOOST_PP_STRINGIZE(item) << ": " << item
#define LOG_CTX_TOP_L_ITEM__2(item) \
   LOG_CTX_TOP_L_ITEM__sep << BOOST_PP_TUPLE_ELEM(0, item) << ": " \
      << BOOST_PP_TUPLE_ELEM(1, item)
#define LOG_CTX_TOP_L_ITEM__3(item) \
    do { \
       if (_log_level >= BOOST_PP_CAT(Log_, BOOST_PP_TUPLE_ELEM(0, item))) { \
         LOG_CTX_TOP_L_ITEM__sep << BOOST_PP_TUPLE_ELEM(1, item) << ": " \
            << BOOST_PP_TUPLE_ELEM(2, item); \
       } \
    } while (0)
#define LOG_CTX_TOP_L_ITEM__bad(item) \
   static_assert(false, "wrong number of arguments in log info, is " BOOST_PP_STRINGIZE(item))

#define LOG_CTX_TOP_L_ITEM(r, data, item) \
   BOOST_PP_IF( \
      BOOST_PP_IS_BEGIN_PARENS(item), \
      BOOST_PP_IF( \
         BOOST_PP_EQUAL(BOOST_PP_TUPLE_SIZE(item), 2), \
         LOG_CTX_TOP_L_ITEM__2, \
         BOOST_PP_IF( \
            BOOST_PP_EQUAL(BOOST_PP_TUPLE_SIZE(item), 3), \
            LOG_CTX_TOP_L_ITEM__3, \
            LOG_CTX_TOP_L_ITEM__bad)), \
      LOG_CTX_TOP_L_ITEM__1)(item);

#define LOG_CTX_TOP_L_ITEMS(items) \
   do { \
      BOOST_PP_SEQ_FOR_EACH(LOG_CTX_TOP_L_ITEM, , items) \
   } while (0)

#define LOG_CTX_TOP_L(Topic, Level, Context, Line, Message, ...) \
   do { \
      Logger* const _log_logger = Logger::getLogger(); \
      if (!_log_logger || _log_logger->getLogLevel(Topic) < Level) \
         break; \
      auto _log_level = _log_logger->getLogLevel(Topic); \
      (void) _log_level; \
      unsigned _log_items = 0; \
      (void) _log_items; \
      beegfs::logging::LogInfos _log_line; \
      const beegfs::logging::SystemError sysErr; (void) sysErr; \
      const beegfs::logging::InBase<std::hex> hex; (void) hex; \
      const beegfs::logging::InBase<std::oct> oct; (void) oct; \
      _log_line << Message; \
      LOG_CTX_TOP_L_ITEMS( \
            BOOST_PP_SEQ_POP_FRONT(BOOST_PP_TUPLE_TO_SEQ((, ##__VA_ARGS__)))); \
      _log_logger->log(Topic, Level, Context, Line, _log_line.str().c_str()); \
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
