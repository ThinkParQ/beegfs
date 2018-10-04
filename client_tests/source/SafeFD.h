#ifndef source_SafeFD_h_e5imImnVyDN0JAztTaLork
#define source_SafeFD_h_e5imImnVyDN0JAztTaLork

#include <string>
#include <memory>
#include <unistd.h>
#include <fcntl.h>

class FileUnlinker;

class SafeFD {
   public:
      explicit SafeFD(int fd)
         : fd(fd)
      {}

      SafeFD()
         : fd(-1)
      {}

      ~SafeFD()
      {
         if(fd >= 0)
            ::close(fd);
      }

      SafeFD(SafeFD&& other);
      SafeFD& operator=(SafeFD&& other);

      static SafeFD open(const std::string& file, int flags);
      static SafeFD openAt(const SafeFD& dir, const std::string& file, int flags, FileUnlinker* unlinker = nullptr);
      static SafeFD openAt(const std::string& dir, const std::string& file, int flags, FileUnlinker* unlinker = nullptr);

   private:
      int fd;

   public:
      int raw() const
      {
         return this->fd;
      }

      int close()
      {
         int fd = this->fd;
         this->fd = -1;
         return ::close(fd);
      }

      SafeFD dup() const
      {
         int fd = ::dup(this->fd);
         if(fd < 0)
            throw std::runtime_error("could not dup");

         return SafeFD(fd);
      }
};

#endif
