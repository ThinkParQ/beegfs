#ifndef FDHANDLE_H
#define FDHANDLE_H

#include <algorithm>
#include <errno.h>
#include <unistd.h>

class FDHandle
{
   public:
      FDHandle(): fd(-1) {}

      explicit FDHandle(int fd): fd(fd) {}

      ~FDHandle()
      {
         if (fd >= 0)
            ::close(fd);
      }

      FDHandle(const FDHandle&) = delete;
      FDHandle& operator=(const FDHandle&) = delete;

      FDHandle(FDHandle&& other): fd(-1)
      {
         swap(other);
      }

      FDHandle& operator=(FDHandle&& other)
      {
         FDHandle(std::move(other)).swap(*this);
         return *this;
      }

      void swap(FDHandle& other)
      {
         std::swap(fd, other.fd);
      }

      int get() const { return fd; }
      int operator*() const { return fd; }
      bool valid() const { return fd >= 0; }

      int close()
      {
         if (fd < 0)
            return 0;

         const int result = ::close(fd);
         fd = -1;

         return result < 0 ? errno : 0;
      }

   private:
      int fd;
};

#endif
