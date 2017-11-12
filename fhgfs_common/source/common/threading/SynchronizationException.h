#ifndef SYNCHRONIZATIONEXCEPTION_H_
#define SYNCHRONIZATIONEXCEPTION_H_

#include "common/toolkit/NamedException.h"
#include "common/Common.h"

DECLARE_NAMEDEXCEPTION(SynchronizationException, "SynchronizationException")

/*
class SynchronizationException : public NamedException
{
   public:
      SynchronizationException(const char* message) :
         NamedException("SynchronizationException", message)
      {
      }

      SynchronizationException(const std::string message) :
         NamedException("SynchronizationException", message)
      {
      }

};
*/

#endif /*SYNCHRONIZATIONEXCEPTION_H_*/
