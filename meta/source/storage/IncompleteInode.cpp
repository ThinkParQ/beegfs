#include "IncompleteInode.h"

#include <common/app/log/Logger.h>
#include <program/Program.h>

#include <sys/xattr.h>
#include <sys/types.h>
#include <unistd.h>

IncompleteInode::~IncompleteInode()
{
   if (fd >= 0 && close(fd) < 0)
      LOG(GENERAL, ERR, "Failed to close file", fd, sysErr);
}

FhgfsOpsErr IncompleteInode::setXattr(const char* name, const void* value, size_t size)
{
   int setRes = ::fsetxattr(fd, name, value, size, 0);
   if (setRes != 0)
      return FhgfsOpsErrTk::fromSysErr(errno);

   xattrsSet.insert(name);
   return FhgfsOpsErr_SUCCESS;
}

FhgfsOpsErr IncompleteInode::setContent(const void* value, size_t size)
{
   if (Program::getApp()->getConfig()->getStoreUseExtendedAttribs())
      return setXattr(META_XATTR_NAME, value, size);

   if (hasContent)
   {
      int truncRes = ::ftruncate(fd, size);
      if (truncRes)
         return FhgfsOpsErrTk::fromSysErr(errno);
   }

   ssize_t writeRes = ::write(fd, value, size);
   if (writeRes < 0 || size_t(writeRes) < size)
      return FhgfsOpsErrTk::fromSysErr(errno);

   hasContent = true;

   return FhgfsOpsErr_SUCCESS;
}

FhgfsOpsErr IncompleteInode::clearUnsetXAttrs()
{
   std::set<std::string> xattrsOnInode;
   char xattrNameBuf[XATTR_LIST_MAX];

   const ssize_t listRes = ::flistxattr(fd, xattrNameBuf, sizeof(xattrNameBuf));
   if (listRes < 0)
   {
      LOG(GENERAL, ERR, "could not list xattrs", fileName(), sysErr);
      return FhgfsOpsErr_INTERNAL;
   }

   for (ssize_t offset = 0; offset < listRes; )
   {
      std::string currentName = xattrNameBuf + offset;

      offset += currentName.size() + 1;
      xattrsOnInode.insert(std::move(currentName));
   }

   for (auto it = xattrsOnInode.begin(); it != xattrsOnInode.end(); it++)
   {
      if (xattrsSet.count(*it))
         continue;
      // do not sync the system namespaces.
      //  * security. cannot be synced at all, since userspace can't write to it
      //  * trusted. is unused by us and thus not interesting
      //  * system. contains only acls, which we don't use to represent metadata
      if (it->compare(0, 5, "user.") != 0)
         continue;

      int removeRes = ::fremovexattr(fd, it->c_str());
      if (removeRes != 0)
      {
         LOG(GENERAL, ERR, "could not remove superfluous xattr",
             fileName(), ("name", *it), sysErr);
         return FhgfsOpsErr_INTERNAL;
      }
   }

   return FhgfsOpsErr_SUCCESS;
}

std::string IncompleteInode::fileName() const
{
   static const char* FD_FORMAT = "/proc/self/fd/%i";

   // reserve enough space for the entire format string, the trailing \0, and the fd value
   // in octal. in decimal, that will be more than enough.
   char buffer[strlen(FD_FORMAT) + 3 * sizeof(fd) + 1];
   ::sprintf(buffer, FD_FORMAT, fd);

   std::string result;
   result.resize(PATH_MAX + 1);

   ssize_t readRes = ::readlink(buffer, &result[0], PATH_MAX);
   if (readRes < 0)
   {
      LOG(GENERAL, ERR, "Failed to resolve file for fd", fd, sysErr);
      return "<resolve error>";
   }

   result.resize(readRes);
   return result;
}
