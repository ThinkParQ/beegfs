#ifndef SOCKETINTERRUPTEDPOLLEXCEPTION_H_
#define SOCKETINTERRUPTEDPOLLEXCEPTION_H_

#include "SocketException.h"
#include "common/Common.h"

DECLARE_NAMEDSUBEXCEPTION(SocketInterruptedPollException, "SocketInterruptedPollException",
   SocketException)

#endif /*SOCKETINTERRUPTEDPOLLEXCEPTION_H_*/
