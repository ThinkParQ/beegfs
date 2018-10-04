#ifndef FILEDESCRIPTOR_H_
#define FILEDESCRIPTOR_H_

#include <common/Common.h>
#include "poll/Pollable.h"

class FileDescriptor : public Pollable
{
   public:
      /**
       * @param fd not owned by this object (will not be closed in the destructor)
       * @param threadsafety true to synchronize read/write access
       */
      FileDescriptor(int fd, bool threadsafety)
      {
         this->fd = fd;
         this->threadsafety = threadsafety;
      }

      virtual ~FileDescriptor() {}


   private:
      int fd;

      bool threadsafety;
      Mutex mutex;


   public:
      // inliners
      ssize_t readExact(void *buf, size_t count)
      {
         size_t missing = count;

         if(threadsafety)
            mutex.lock();

         do
         {
            ssize_t recvRes = ::read(fd, &((char*)buf)[count-missing], missing);
            missing -= recvRes;
         } while(missing);

         if(threadsafety)
            mutex.unlock();

         return (ssize_t)count;
      }

      ssize_t read(void *buf, size_t count)
      {
         if(threadsafety)
            mutex.lock();

         ssize_t readRes = ::read(fd, buf, count);

         if(threadsafety)
            mutex.unlock();

         return readRes;
      }

      ssize_t write(const void *buf, size_t count)
      {
         if(threadsafety)
            mutex.lock();

         ssize_t writeRes = ::write(fd, buf, count);

         if(threadsafety)
            mutex.unlock();

         return writeRes;
      }

      // getters & setters
      virtual int getFD() const
      {
         return fd;
      }
};

#endif /*FILEDESCRIPTOR_H_*/
