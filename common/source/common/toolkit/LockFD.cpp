#include "LockFD.h"

#include <common/system/System.h>

#include <sys/file.h>

#include <iostream>

nu::error_or<LockFD> LockFD::lock(std::string path, bool forUpdate)
{
   FDHandle fd(open(path.c_str(), O_CREAT | O_TRUNC | (forUpdate ? O_RDWR : O_RDONLY), 0644));
   if (!fd.valid())
      return std::error_code{errno, std::system_category()};

   if (flock(*fd, LOCK_EX | LOCK_NB) == -1)
      return std::error_code{errno, std::system_category()};

   return LockFD(std::move(path), std::move(fd));
}

std::error_code LockFD::update(const std::string& newContent)
{
   const int truncRes = ftruncate(*fd, 0);
   if (truncRes == -1)
      return {errno, std::system_category()};

   const ssize_t writeRes = pwrite(*fd, newContent.c_str(), newContent.length(), 0);
   if (writeRes < 0 || size_t(writeRes) != newContent.length())
      return {errno, std::system_category()};

   return {};
}

std::error_code LockFD::updateWithPID()
{
   // add newline, because some shell tools don't like it when there is no newline
   return update(std::to_string(getpid()) + '\n');
}
