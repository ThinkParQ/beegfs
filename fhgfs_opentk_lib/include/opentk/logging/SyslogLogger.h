#ifndef SYSLOGLOGGER_H_
#define SYSLOGLOGGER_H_

#include <opentk/OpenTk_Common.h>

#include <sys/socket.h>
#include <sstream>


/**
 * This class provides a way to send msgs to syslog.
 *
 * Other than the standard ::syslog() method, this class avoids the risk of stalling when the other
 * end of the log device is stalled, based on non-blocking sockets.
 *
 * Note that this class has is an initOnce() method that needs to be called.
 */
class SyslogLogger
{
   public:
      static void initOnce(const char* daemonName);
      static void destroyOnce();

      static int log(unsigned logLevel, const char* msgFormat, ...);


   private:
      SyslogLogger() {}

      static std::string daemonName; // will be included in log msgs

      static int sendBufToSyslog(const char* buf, size_t bufLen);

      static ssize_t sendtoWithRetries(int sockFD, const void* buf, size_t bufLen, int flags,
         const struct sockaddr* destAddr, socklen_t addrLen);
};


#endif /* SYSLOGLOGGER_H_ */
