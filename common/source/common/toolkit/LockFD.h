#ifndef COMMON_LOCKFD_H
#define COMMON_LOCKFD_H

#include <common/toolkit/FDHandle.h>

#include <nu/error_or.hpp>

class LockFD final
{
   public:
      LockFD() = default;
      ~LockFD()
      {
         // unlink and close could also happen in the other order; both effectively release the
         // lock file for use by others. only unlink if the fd is valid - the path have been reused.
         if (fd.valid())
            unlink(path.c_str());
      }

      LockFD(const LockFD&) = delete;
      LockFD& operator=(const LockFD&) = delete;

      LockFD(LockFD&& other)
      {
         swap(other);
      }

      LockFD& operator=(LockFD&& other)
      {
         LockFD(std::move(other)).swap(*this);
         return *this;
      }

      static nu::error_or<LockFD> lock(std::string path, bool forUpdate);

      std::error_code update(const std::string& newContent);
      std::error_code updateWithPID();

      bool valid() const { return fd.valid(); }

      void swap(LockFD& other)
      {
         using std::swap;
         swap(path, other.path);
         swap(fd, other.fd);
      }

      friend void swap(LockFD& a, LockFD& b)
      {
         a.swap(b);
      }

   private:
      std::string path;
      FDHandle fd;

      explicit LockFD(std::string path, FDHandle fd): path(std::move(path)), fd(std::move(fd)) {}
};

#endif
