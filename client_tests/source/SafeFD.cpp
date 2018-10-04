#include "SafeFD.h"
#include <unistd.h>
#include <fcntl.h>
#include <stdexcept>
#include <algorithm>
#include "FileUnlinker.h"

SafeFD::SafeFD(SafeFD&& other)
   : fd(other.fd)
{
   other.fd = -1;
}

SafeFD& SafeFD::operator=(SafeFD&& other)
{
   std::swap(this->fd, other.fd);
   return *this;
}

SafeFD SafeFD::open(const std::string& file, int flags)
{
   SafeFD result(::open(file.c_str(), flags, 0666));

   if(result.raw() < 0)
      throw std::runtime_error("could not open " + file);

   return result;
}

SafeFD SafeFD::openAt(const SafeFD& dir, const std::string& file, int flags, FileUnlinker* unlinker)
{
   SafeFD result(::openat(dir.raw(), file.c_str(), flags, 0666));

   if(result.raw() < 0)
      throw std::runtime_error("could not open " + file);

   if(unlinker)
      *unlinker = FileUnlinker(dir.dup(), file);

   return result;
}

SafeFD SafeFD::openAt(const std::string& dir, const std::string& file, int flags, FileUnlinker* unlinker)
{
   auto dirFD = open(dir, O_DIRECTORY);

   if(dirFD.raw() < 0)
      throw std::runtime_error("could not open " + dir);

   return openAt(dirFD, file, flags, unlinker);
}
