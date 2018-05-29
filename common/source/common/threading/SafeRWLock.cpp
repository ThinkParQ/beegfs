#include <common/app/log/LogContext.h>
#include "SafeRWLock.h"


void SafeRWLock::errRWLockStillLocked()
{
   LogContext log("SafeRWLock");
   log.logErr("Bug(?): Application did not unlock a rwlock!");
   log.logBacktrace();
}


void SafeRWLock::errRWLockAlreadyUnlocked()
{
   LogContext log("SafeRWLock");
   log.logErr("Bug(?): Application tried to unlock a rwlock that has already been unlocked!");
   log.logBacktrace();
}


void SafeRWLock::errRWLockAlreadyLocked()
{
   LogContext log("SafeRWLock");
   log.logErr("Bug(?): Application tried to lock a rwlock that has already been locked!");
   log.logBacktrace();
}


