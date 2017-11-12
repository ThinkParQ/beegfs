#ifndef SEMAPHOREEXCEPTION_H_
#define SEMAPHOREEXCEPTION_H_

#include "SynchronizationException.h"

DECLARE_NAMEDSUBEXCEPTION(SemaphoreException, "SemaphoreException", SynchronizationException)

/*
class SemaphoreException : public SynchronizationException
{
   public:
      SemaphoreException(const char* message) :
         SynchronizationException(message)
      {
      }
      
      SemaphoreException(const std::string message) :
         SynchronizationException(message)
      {
      }
};
*/

#endif /*SEMAPHOREEXCEPTION_H_*/
