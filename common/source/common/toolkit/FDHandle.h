#pragma once

#include <algorithm>
#include <errno.h>
#include <unistd.h>

class FDHandle
{
   private:
      int m_fd = -1;

   public:
      int close()
      {
         if (! valid())
            return 0;

         int r = ::close(m_fd);
         m_fd = -1;
         return r;
      }

      int get() const
      {
         return m_fd;
      }

      void reset(int fd)
      {
         close();
         m_fd = fd;
      }

      bool valid() const
      {
         return m_fd >= 0;
      }

      // so weird... can we get rid of this?
      int operator*() const
      {
         return m_fd;
      }

      FDHandle& operator=(const FDHandle&) = delete;

      void operator=(FDHandle&& other)
      {
         close();
         std::swap(m_fd, other.m_fd);
      }

      FDHandle()
      {}

      explicit FDHandle(int fd)
      {
         reset(fd);
      }

      FDHandle(const FDHandle&) = delete;

      FDHandle(FDHandle&& other)
      {
         std::swap(m_fd, other.m_fd);
      }

      ~FDHandle()
      {
         close();
      }
};
