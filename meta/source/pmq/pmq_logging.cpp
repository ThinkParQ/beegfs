#include "pmq_logging.hpp"
#include "pmq_common.hpp"

#include <cassert>
#include <cstdio>


// The logging module can either print to stderr or use the BeeGFS metadata
// server's logging backend.

#ifdef PMQ_TEST
# ifndef PMQ_LOG_LEVEL
#  error PMQ_LOG_LEVEL must be defined when compiling test case
# endif
#define INTEGRATE_WITH_METADATA_SERVER 0
#else
#define INTEGRATE_WITH_METADATA_SERVER 1
#endif


#if INTEGRATE_WITH_METADATA_SERVER
// Integrate into metadata server
#include <common/app/log/Logger.h>
#endif

struct Log_Buffer
{
   PMQ_PROFILED_MUTEX(mutex);
   PMQ_PROFILED_CONDVAR(writeable);  // reader => writer
   PMQ_PROFILED_CONDVAR(readable);   // writer => reader. Corresponding mutex is in Log_Message
   Alloc_Slice<Log_Message> msgs;
   size_t capacity = 0;
   size_t writepos = 0;
   size_t readpos = 0;

   Log_Buffer()
   {
      // TODO: this costs a lot of memory. However, a previous setting of 64
      // wasn't enough for high-frequency logging. We need more dynamic and
      // judicious memory allocation.
      capacity = 1024;
      msgs.allocate(capacity);
   }
};

static void log_buffer_write(Log_Buffer *logbuf, Log_Message const *input)
{
   PMQ_PROFILED_UNIQUE_LOCK(lock, logbuf->mutex);
   while (logbuf->writepos - logbuf->readpos == logbuf->capacity)
      logbuf->writeable.wait(lock);
   size_t mask = logbuf->capacity - 1;
   size_t pos = logbuf->writepos;
   Log_Message *msg = &logbuf->msgs[pos & mask];
   msg->size = input->size;
   memcpy(msg->data, input->data, input->size);
   ++ logbuf->writepos;
   // hoping that this is cheap: otherwise we should track the number of
   // readers and check it before calling notify_one()
   logbuf->readable.notify_one();
}

static void _log_buffer_read(Log_Buffer *logbuf, Log_Message *output)
{
   size_t mask = logbuf->capacity - 1;
   size_t pos = logbuf->readpos;
   Log_Message *msg = &logbuf->msgs[pos & mask];
   output->size = msg->size;
   memcpy(output->data, msg->data, msg->size);
   ++ logbuf->readpos;
   // hoping that this is cheap: otherwise we should track the number of
   // writers and check it before calling notify_one()
   logbuf->writeable.notify_one();
}

static void log_buffer_read(Log_Buffer *logbuf, Log_Message *output)
{
   PMQ_PROFILED_UNIQUE_LOCK(lock, logbuf->mutex);
   while (logbuf->writepos == logbuf->readpos)
      logbuf->readable.wait(lock);
   _log_buffer_read(logbuf, output);
}

static bool log_buffer_try_read(Log_Buffer *logbuf, Log_Message *output)
{
   PMQ_PROFILED_LOCK(lock, logbuf->mutex);
   if (logbuf->writepos == logbuf->readpos)
      return false;
   _log_buffer_read(logbuf, output);
   return true;
}

static bool log_buffer_try_read_timeout_millis(Log_Buffer *logbuf, Log_Message *output, int millis)
{
   auto time_point = std::chrono::steady_clock::now() + std::chrono::milliseconds(millis);
   PMQ_PROFILED_UNIQUE_LOCK(lock, logbuf->mutex);
   while (logbuf->writepos == logbuf->readpos)
   {
      if (logbuf->readable.wait_until(lock, time_point) == std::cv_status::timeout)
         return false;
   }
   _log_buffer_read(logbuf, output);
   return true;
}



static Log_Buffer global_log_buffer;

void pmq_write_log_message(Log_Message const *input)
{
   log_buffer_write(&global_log_buffer, input);
}

void pmq_read_log_message(Log_Message *output)
{
   log_buffer_read(&global_log_buffer, output);
}

bool pmq_try_read_log_message(Log_Message *output)
{
   return log_buffer_try_read(&global_log_buffer, output);
}

bool pmq_try_read_log_message_timeout_millis(Log_Message *output, int millis)
{
   return log_buffer_try_read_timeout_millis(&global_log_buffer, output, millis);
}


void log_msg_printfv(Log_Message *msg, const char *fmt, va_list ap)
{
   int ret = vsnprintf(msg->data + msg->size, sizeof msg->data - 1 - msg->size, fmt, ap);
   assert(ret >= 0);
   msg->size += (size_t) ret;
   if (msg->size > sizeof msg->data - 1)
      msg->size = sizeof msg->data - 1;
   msg->data[msg->size] = 0;
}

// Note: this is a method (implicit this pointer) so we use
// __pmq_formatter(2, 3) instead of __pmq_formatter(1, 2).
void __pmq_formatter(2, 3) log_msg_printf(Log_Message *msg, const char *fmt, ...)  // NOLINT this is safe because of __pmq_formatter() annotation
{
   va_list ap;
   va_start(ap, fmt);
   log_msg_printfv(msg, fmt, ap);
   va_end(ap);
}

void pmq_msg_ofv(const PMQ_Msg_Options& opt, const char *fmt, va_list ap)
{
   bool print_errno = (bool) (opt.flags & PMQ_MSG_OPT_ERRNO);
   uint32_t priority = opt.flags & PMQ_MSG_OPT_LVL_MASK;

   Log_Message log_msg;
   log_msg.size = 0;

#if INTEGRATE_WITH_METADATA_SERVER

   int metadata_priority = 0;

   switch (priority)
   {
   case PMQ_MSG_OPT_LVL_DEBUG: metadata_priority = Log_DEBUG; break;
   case PMQ_MSG_OPT_LVL_INFO:  metadata_priority = Log_NOTICE; break;
   case PMQ_MSG_OPT_LVL_WARN:  metadata_priority = Log_WARNING; break;
   case PMQ_MSG_OPT_LVL_ERR:   metadata_priority = Log_ERR; break;
   default: assert(0); // can't happen at least currently where log mask has 2 bits.
   }

#else

   // Early return, avoiding most of the work if the message has less priority
   // than the log level.
   // TODO: we should have something like this for metadata server integration
   // too.
   if (PMQ_LOG_LEVEL > priority)
      return;

   switch (priority)
   {
   case PMQ_MSG_OPT_LVL_DEBUG: log_msg_printf(&log_msg, "DEBUG: "); break;
   case PMQ_MSG_OPT_LVL_INFO:  log_msg_printf(&log_msg, "INFO: "); break;
   case PMQ_MSG_OPT_LVL_WARN:  log_msg_printf(&log_msg, "WARNING: "); break;
   case PMQ_MSG_OPT_LVL_ERR:   log_msg_printf(&log_msg, "ERROR: "); break;
   default: assert(0); // can't happen at least currently where log mask has 2 bits.
   }

#endif

   log_msg_printfv(&log_msg, fmt, ap);

   if (print_errno)
   {
      char errbuf[64];
      const char *errstr;

#if  (_POSIX_C_SOURCE >= 200112L) && !  _GNU_SOURCE
      {
         // XSI compliant strerror_r()
         int ret = strerror_r(opt.errnum, errbuf, sizeof errbuf);
         if (ret == 0)
            errstr = errbuf;
      }
#else
      {
         // GNU version of strerror_r()
         errstr = strerror_r(opt.errnum, errbuf, sizeof errbuf);
      }
#endif
      if (! errstr)
      {
         snprintf(errbuf, sizeof errbuf, "(errno=%d)", opt.errnum);
         errstr = errbuf;
      }

      log_msg_printf(&log_msg, ": %s", errstr);
   }

#if INTEGRATE_WITH_METADATA_SERVER
   // Integration into metadata server
   Logger *logger = Logger::getLogger();
   logger->log(LogTopic_EVENTLOGGER, metadata_priority, opt.loc.file, opt.loc.line, log_msg.data);
#else

   log_msg_printf(&log_msg, "\n");

   //fwrite(log_msg.data, log_msg.size, 1, stderr);

   pmq_write_log_message(&log_msg);

#endif
}

void __pmq_formatter(2, 3) pmq_msg_of(const PMQ_Msg_Options& opt, const char *fmt, ...)  // NOLINT this is safe because of use of __pmq_formatter() annotation
{
   va_list ap;
   va_start(ap, fmt);
   pmq_msg_ofv(opt, fmt, ap);
   va_end(ap);
}
