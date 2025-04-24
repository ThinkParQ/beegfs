#pragma once

#include "SocketException.h"
#include "common/Common.h"

DECLARE_NAMEDSUBEXCEPTION(SocketInterruptedPollException, "SocketInterruptedPollException",
   SocketException)

