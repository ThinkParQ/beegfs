#ifndef SEMAPHORE_H_
#define SEMAPHORE_H_

#include "SemaphoreException.h"
#include <common/system/System.h>
#include <common/Common.h>

#include <semaphore.h>


class Semaphore
{
   public:
      Sempaphore(unsigned int value)
      {
         if(sem_init(&semaphore, 0, value) )
            throw SemaphoreException(System::getErrString() );
      }
      
      ~Semphore()
      {
         if(sem_destroy(&semaphore) )
            throw SemaphoreException(System::getErrString() );
      }
      
      void sem_wait()
      {
         ::sem_wait(&semaphore);
      }

      void sem_trywait()
      {
         if(::sem_trywait(&semaphore) )
            throw SemaphoreException(System::getErrString() );
      }

      void sem_post()
      {
         if(::sem_post(&semaphore) )
            throw SemaphoreException(System::getErrString() );
      }
      
   
   private:
      sem_t semaphore;
   
   
};


#endif /*SEMAPHORE_H_*/
