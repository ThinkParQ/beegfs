#pragma once

#include "pmq_base.hpp"

enum
{
   PMQ_MSG_OPT_DEFAULT = 0,
   PMQ_MSG_OPT_ERRNO = (1 << 0),
   PMQ_MSG_HAS_SOURCE_LOC = (1 << 1),

   PMQ_MSG_OPT_LVL_MASK = (3 << 2),  // bits 3 and 4 for debug level.
   PMQ_MSG_OPT_LVL_DEBUG = (0 << 2),
   PMQ_MSG_OPT_LVL_INFO = (1 << 2),
   PMQ_MSG_OPT_LVL_WARN = (2 << 2),
   PMQ_MSG_OPT_LVL_ERR = (3 << 2),
};

struct PMQ_Source_Loc
{
   const char *file;
   uint32_t line;
};

struct PMQ_Msg_Options
{
   PMQ_Source_Loc loc;
   uint32_t flags;  // PMQ_MSG_OPT_
   int errnum;
};


// Logging functions / macros.
//
// The following functions are typically used in the client code.
//
//   pmq_msg_f(fmt, ...): Submit a log message with default log level and format string + var-args
//   pmq_perr_f(fmt, ...): Submit a log message with error log level and format string + var-args
//   pmq_perr_ef(errno, fmt, ...): like pmq_perr_f() but also add text for given system error code ("errno")
//
// To explain all functions available here: they are made according to a pattern
//
// pmq_{LVL}_{MNEMNONICS}
//
// LVL: logging level, possible options
//  - msg: Default level or use specified level ('l' mnemnonic)
//  - debug: Debug level
//  - warn: Warning level
//  - perr: Error level ("print-error")
//
// MNEMNONICS: combination of 1-letter chars
//  - l: means a logging level is specified (only available with 'msg' category)
//  - e: add a text for specified system error code ("errno")
//  - f: "format", like in the stdio function printf().
//  - v: in combination with f (so 'fv'), means the arguments come as a va_list, like in stdio function vfprintf().
//  - o: Use a single options struct holding level, errno explictly, as well as source code location info.


void pmq_msg_ofv(const PMQ_Msg_Options& opt, const char *fmt, va_list ap);

void __pmq_formatter(2, 3) pmq_msg_of(const PMQ_Msg_Options& opt, const char *fmt, ...);

#define PMQ_SOURCE_LOC ((PMQ_Source_Loc) { __FILE__, __LINE__ })

#define PMQ_MSG_OPTIONS(...) (PMQ_Msg_Options { PMQ_SOURCE_LOC, ##__VA_ARGS__ })

#define pmq_msg_lf(lvl, fmt, ...) \
   pmq_msg_of(PMQ_MSG_OPTIONS((lvl), 0), fmt, ##__VA_ARGS__)

#define pmq_msg_lef(lvl, e, fmt, ...) \
   pmq_msg_of(PMQ_MSG_OPTIONS(PMQ_MSG_OPT_ERRNO | (lvl), e), fmt, ##__VA_ARGS__)

#define pmq_msg_f(fmt, ...) \
   pmq_msg_lf(PMQ_MSG_OPT_LVL_INFO, fmt, ##__VA_ARGS__)

#define pmq_msg_ef(e, fmt, ...) \
   pmq_msg_lef(PMQ_MSG_OPT_LVL_INFO, (e), fmt, ##__VA_ARGS__)

#define pmq_debug_f(fmt, ...) \
   pmq_msg_lf(PMQ_MSG_OPT_LVL_DEBUG, fmt, ##__VA_ARGS__)

#define pmq_debug_ef(e, fmt, ...) \
   pmq_msg_lef(PMQ_MSG_OPT_LVL_DEBUG, (e), fmt, ##__VA_ARGS__)

#define pmq_warn_f(fmt, ...) \
   pmq_msg_lf(PMQ_MSG_OPT_LVL_WARN, fmt, ##__VA_ARGS__)

#define pmq_warn_ef(e, fmt, ...) \
   pmq_msg_lef(PMQ_MSG_OPT_LVL_WARN, (e), fmt, ##__VA_ARGS__)

#define pmq_perr_f(fmt, ...) \
   pmq_msg_lf(PMQ_MSG_OPT_LVL_ERR, fmt, ##__VA_ARGS__)

#define pmq_perr_ef(e, fmt, ...) \
   pmq_msg_lef(PMQ_MSG_OPT_LVL_ERR, (e), fmt, ##__VA_ARGS__)






// Low-level Logging I/O interface

struct Log_Message
{
   size_t size;
   char data[256 - sizeof (size_t)];  // for simplicity
};

void pmq_write_log_message(Log_Message const *input);
void pmq_read_log_message(Log_Message *output);
bool pmq_try_read_log_message_timeout_millis(Log_Message *output, int millis);
bool pmq_try_read_log_message(Log_Message *output);
