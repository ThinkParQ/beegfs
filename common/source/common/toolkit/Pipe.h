#ifndef PIPE_H_
#define PIPE_H_

#include "FileDescriptor.h"
#include <common/Common.h>

#define PIPE_READFD_INDEX     0
#define PIPE_WRITEFD_INDEX    1

class Pipe
{
   public:
      Pipe(bool threadsafeReadside, bool threadsafeWriteside)
      {
         filedes[PIPE_READFD_INDEX] = -1;
         filedes[PIPE_WRITEFD_INDEX] = -1;

         int pipeRes = pipe(filedes);
         IGNORE_UNUSED_VARIABLE(pipeRes);

         this->readFD = new FileDescriptor(filedes[PIPE_READFD_INDEX], threadsafeReadside);
         this->writeFD = new FileDescriptor(filedes[PIPE_WRITEFD_INDEX], threadsafeWriteside);
      }

      ~Pipe()
      {
         delete(writeFD);
         delete(readFD);

         close(filedes[PIPE_WRITEFD_INDEX]);
         close(filedes[PIPE_READFD_INDEX]);
      }

   private:
      int filedes[2];
      FileDescriptor* readFD;
      FileDescriptor* writeFD;

   public:
      // getters & setters
      FileDescriptor* getReadFD() const
      {
         return readFD;
      }

      FileDescriptor* getWriteFD() const
      {
         return writeFD;
      }

      /**
       * @return true if incoming data is available or error occurred, false if a timeout occurred
       */
      bool waitForIncomingData(int timeoutMS)
      {
            struct pollfd pollStruct = {readFD->getFD(), POLLIN, 0};
            int pollRes = poll(&pollStruct, 1, timeoutMS);

            if( (pollRes > 0) && (pollStruct.revents & POLLIN) )
            {
               return true;
            }
            else
            if(!pollRes)
               return false;
            else
               return true;   //error occurred, user gets the error by a read on the pipe
      }
};

#endif /*PIPE_H_*/
