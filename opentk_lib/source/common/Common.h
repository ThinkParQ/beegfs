#ifndef COMMON_H_
#define COMMON_H_

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <malloc.h>
#include <getopt.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>

#include <opentk/OpenTk_Common.h>


#define BEEGFS_MIN(a, b)    ( ( (a) < (b) ) ? (a) : (b) )
#define BEEGFS_MAX(a, b)    ( ( (a) > (b) ) ? (a) : (b) )


#define SAFE_FREE(p) do {if(p) free(p); } while(0)


// gcc branch optimization hints
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x)  __builtin_expect(!!(x), 0)


#ifdef BEEGFS_OPENTK_LOG_CONN_ERRORS
   #define syslog_fhgfs_connerr(file, fmtStr, ...)    \
      SyslogLogger::log(LOG_WARNING, fmtStr, ## __VA_ARGS__)
#else
   #define syslog_fhgfs_connerr(file, fmtStr, ...) \
      do {} while (0)  /* logging of conn errors disabled */
#endif // BEEGFS_OPENTK_LOG_CONN_ERRORS

#endif /*COMMON_H_*/
