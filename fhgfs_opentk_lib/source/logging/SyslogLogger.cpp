#include <common/Common.h>
#include <opentk/logging/SyslogLogger.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>
#include <stdarg.h>
#include <syslog.h>


#define SYSLOGLOGGER_LOGDEV_PATH          "/dev/log"
#define SYSLOGLOGGER_MAX_MSG_LEN          1024
#define SYSLOGLOGGER_SEND_RETRY_NUM       2 // number of retries if send() to log device failed
#define SYSLOGLOGGER_SEND_RETRY_WAIT_MS   1000 // poll wait time between retries


#ifndef SOCK_NONBLOCK
// old libc/kernels (before 2.6.27) didn't have SOCK_NONBLOCK, but that's not critical
#define SOCK_NONBLOCK   0
#endif // SOCK_NONBLOCK


std::string SyslogLogger::daemonName = "beegfs"; // will be set to real name in initOnce()


/**
 * Call this exactly once before using this class otherwise.
 *
 * Note: When all logging is done at the end of the application, call destroyOnce().
 */
void SyslogLogger::initOnce(const char* daemonName)
{
   SyslogLogger::daemonName = daemonName;
}

/**
 * Call this exactly once after all logging is done at the end of the application.
 */
void SyslogLogger::destroyOnce()
{
   // nothing to be done here, currently
}

/**
 * Send a message to syslog.
 *
 * @param logLevel LOG_... level (as defined in "man 3 syslog").
 * @param msgFormat similar to printf's "format" with following arguments.
 * @return -1 on error, in which case msg is logged to stderr.
 */
int SyslogLogger::log(unsigned logLevel, const char* msgFormat, ...)
{
   va_list ap; // additional parameters

   // preprocessing: write given msg to buf

   char msgFormatBuf[SYSLOGLOGGER_MAX_MSG_LEN];

   va_start(ap, msgFormat);

   int printMsgRes = vsnprintf(msgFormatBuf, sizeof(msgFormatBuf), msgFormat, ap);

   if(printMsgRes >= (int)sizeof(msgFormatBuf) )
   { // msg too long for our buffer
      msgFormatBuf[sizeof(msgFormatBuf)-1] = 0; // add terminating zero
   }

   va_end(ap);

   // generate actual syslog buf: prepend syslog "header" to preprocessed msg buf

   char syslogBuf[SYSLOGLOGGER_MAX_MSG_LEN];

   /* from <syslog.h>: priorities/facilities are encoded into a single 32-bit quantity, where the
      bottom 3 bits are the priority (0-7) and the top 28 bits are the facility number.
      (facility number values are already shifted by 3 bits.) */

   int printLogRes = snprintf(syslogBuf, sizeof(syslogBuf), "<%u>%s[%u]: %s",
      logLevel | LOG_DAEMON, daemonName.c_str(), getpid(), msgFormatBuf);

   if(printLogRes >= (int)sizeof(syslogBuf) )
   { // msg too long for our buffer
      syslogBuf[sizeof(syslogBuf)-1] = 0; // add terminating zero
      printLogRes = sizeof(syslogBuf)-1; // adapt for send, not including terminating 0
   }

   int sendRes = sendBufToSyslog(syslogBuf, printLogRes + 1);

   if(sendRes < 0)
      fprintf(stderr, "[Undeliverable syslog msg] %s\n", msgFormatBuf);

   return sendRes;
}


/**
 * Send a prepared string buffer to the log device.
 *
 * @return number of sent bytes on success, <=0 on error
 */
int SyslogLogger::sendBufToSyslog(const char* buf, size_t bufLen)
{
   /* we don't know if the log device is of type SOCK_STREAM or SOCK_DGRAM, so try stream first and
      fallback to dgram if a protocol error is reported (similar to glibc's approach). */

   int sockFD;
   struct sockaddr_un address;
   ssize_t sendRes;

   // (use SOCK_NONBLOCK for stream socket to avoid blocking on connect() )
   sockFD = socket(PF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
   if(sockFD == -1)
   {
      perror("socket()");
      return -1;
   }

   // init log device address
   memset(&address, 0, sizeof(struct sockaddr_un) );
   address.sun_family = AF_UNIX;
   snprintf(address.sun_path, sizeof(address.sun_path), SYSLOGLOGGER_LOGDEV_PATH);

   int connectRes = connect(sockFD,
      (struct sockaddr*)&address,  sizeof(struct sockaddr_un) );

   if(!connectRes)
   { // stream connection succeeded => send msg
      sendRes = sendtoWithRetries(sockFD, buf, bufLen, MSG_NOSIGNAL | MSG_DONTWAIT,
         NULL, 0);
   }
   else
   if(errno == EPROTOTYPE)
   { // stream is wrong protocol, so this seems to be a datagram socket
      close(sockFD);

      sockFD = socket(PF_UNIX, SOCK_DGRAM, 0);
      if(sockFD < 0)
      {
         perror("socket()");
         return -1;
      }

      sendRes = sendtoWithRetries(sockFD, buf, bufLen, MSG_NOSIGNAL | MSG_DONTWAIT,
         (struct sockaddr*)&address,  sizeof(struct sockaddr_un) );
   }
   else
   { // unrecoverable error
      static bool connectErrLogged = false; // to avoid log spamming

      if(!connectErrLogged)
      {
         connectErrLogged = true;
         perror("connect() to syslog device");
      }

      close(sockFD);
      return -1;
   }

   if(sendRes < 0)
   { // send failed
      static bool sendErrLogged = false; // to avoid log spamming

      if(!sendErrLogged)
      {
         sendErrLogged = true;
         perror("send() to syslog device");
      }

      close(sockFD);
      return -1;
   }

   close(sockFD);

   return 0;
}

/**
 * Wrapper for normal ::sendto(), which also does retries with timeouts based on poll(), in case
 * a non-blocking ::sendto() call failed.
 *
 * @return number of sent bytes on success, <0 otherwise
 */
ssize_t SyslogLogger::sendtoWithRetries(int sockFD, const void* buf, size_t bufLen, int flags,
   const struct sockaddr* destAddr, socklen_t addrLen)
{
   unsigned numRetriesLeft = SYSLOGLOGGER_SEND_RETRY_NUM;
   unsigned resendWaitTimeoutMS = SYSLOGLOGGER_SEND_RETRY_WAIT_MS;

   for( ; ; )
   {
      ssize_t sendRes = sendto(sockFD, buf, bufLen, flags, destAddr, addrLen);
      if(sendRes > 0)
         return sendRes; // msg successfully delivered

      if( (sendRes == -1) && (errno == EAGAIN) && --numRetriesLeft)
      { // try waiting for the log dev to become ready to receive data
         struct pollfd pollFD;
         pollFD.fd = sockFD;
         pollFD.events = POLLOUT;
         pollFD.revents = 0;

         int pollRes = poll(&pollFD, 1, resendWaitTimeoutMS);
         if(pollRes == -1)
            return pollRes; // unrecoverable error

         // try to send again...
         continue;
      }

      // unrecoverable error or retry limit exceeded
      return sendRes;
   }
}
