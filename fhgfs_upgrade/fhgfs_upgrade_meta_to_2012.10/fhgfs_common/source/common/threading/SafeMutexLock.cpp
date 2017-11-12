#include <common/app/log/LogContext.h>
#include "SafeMutexLock.h"


void SafeMutexLock::errMutexStillLocked()
{
   LogContext log("SafeMutexLock");
   log.logErr("Bug(?): Application did not unlock a mutex!");
   log.logBacktrace();
}


void SafeMutexLock::errMutexAlreadyUnlocked()
{
   LogContext log("SafeMutexLock");
   log.logErr("Bug(?): Application tried to unlock a mutex that has already been unlocked!");
   log.logBacktrace();
}


void SafeMutexLock::errMutexAlreadyLocked()
{
   LogContext log("SafeMutexLock");
   log.logErr("Bug(?): Application tried to lock a mutex that has already been locked!");
   log.logBacktrace();
}


