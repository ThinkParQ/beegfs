#include "XAttrTk.h"

#include <app/App.h>
#include <common/app/log/Logger.h>
#include <program/Program.h>

#include <array>
#include <memory>

#include <sys/xattr.h>
#include <cerrno>

namespace XAttrTk
{

const std::string UserXAttrPrefix("user.bgXA.");


std::pair<FhgfsOpsErr, std::vector<std::string>> listXAttrs(const std::string& path)
{
   ssize_t size = ::listxattr(path.c_str(), NULL, 0); // get size of raw list

   if(size < 0)
   {
      LOG(GENERAL, ERR, "listxattr failed", path, sysErr);
      return {FhgfsOpsErr_INTERNAL, {}};
   }

   std::unique_ptr<char[]> xAttrRawList(new (std::nothrow) char[size]);

   if (!xAttrRawList)
      return {FhgfsOpsErr_INTERNAL, {}};

   size = ::listxattr(path.c_str(), xAttrRawList.get(), size); // get actual raw list

   if (size >= 0)
   {
      std::vector<std::string> names;

      StringTk::explode(std::string(xAttrRawList.get(), size), '\0', &names);
      return {FhgfsOpsErr_SUCCESS, std::move(names)};
   }

   int err = errno;

   switch (err)
   {
      case ERANGE:
      case E2BIG:
         return {FhgfsOpsErr_RANGE, {}};

      case ENOENT:
         return {FhgfsOpsErr_PATHNOTEXISTS, {}};

      default: // don't forward other errors to the client, but log them on the server
         LOG(GENERAL, ERR, "listxattr failed", path, sysErr);
         return {FhgfsOpsErr_INTERNAL, {}};
   }
}

std::tuple<FhgfsOpsErr, std::vector<char>, ssize_t> getXAttr(const std::string& path,
   const std::string& name, size_t maxSize)
{
   std::vector<char> result(maxSize, 0);

   ssize_t size = getxattr(path.c_str(), name.c_str(), &result.front(), result.size());

   if (size >= 0)
   {
      result.resize(std::min<size_t>(size, maxSize));
      return std::make_tuple(FhgfsOpsErr_SUCCESS, std::move(result), size);
   }

   switch (errno)
   {
      case ENODATA:
         return std::make_tuple(FhgfsOpsErr_NODATA, std::vector<char>(), ssize_t(0));

      case ERANGE:
      case E2BIG:
         return std::make_tuple(FhgfsOpsErr_RANGE, std::vector<char>(), ssize_t(0));

      case ENOENT:
         return std::make_tuple(FhgfsOpsErr_PATHNOTEXISTS, std::vector<char>(), ssize_t(0));

      default: // don't forward other errors to the client, but log them on the server
         LOG(GENERAL, ERR, "getxattr failed", path, name, sysErr);
         return std::make_tuple(FhgfsOpsErr_INTERNAL, std::vector<char>(), ssize_t(0));
   }
}

void sanitizeForUser(std::vector<std::string>& names)
{
   removeMetadataAttrs(names);

   for (auto it = names.begin(); it != names.end(); ++it)
      it->erase(0, UserXAttrPrefix.size());
}

static bool isMetaAttr(const std::string& attrName)
{
   return attrName.compare(0, UserXAttrPrefix.size(), UserXAttrPrefix) != 0;
}

void removeMetadataAttrs(std::vector<std::string>& names)
{
   names.erase(
         std::remove_if(names.begin(), names.end(), isMetaAttr),
         names.end());
}

FhgfsOpsErr setUserXAttr(const std::string& path, const std::string& name, const void* value,
   size_t size, int flags)
{
   const bool limitXAttrListLength = Program::getApp()->getConfig()->getLimitXAttrListLength();

   int res = limitXAttrListLength
      ? setxattr(path.c_str(), (UserXAttrPrefix + name).c_str(), value, size, flags | XATTR_REPLACE)
      : setxattr(path.c_str(), (UserXAttrPrefix + name).c_str(), value, size, flags);

   // if xattr list length is limited and we were not able to replace the attribute, we have to
   // create a new one. (the user may specify XATTR_REPLACE as well, so that has to be checked for)
   // if we have to create a new attribute, we must ensure that the xattr list length after the
   // creation of the attribute does not exceed XATTR_LIST_MAX.
   // use a large hash of mutexes to exclude concurrent setxattr operations on the same path.
   // ideally we would want to use one mutex per worker thread, and ensure that each path has its
   // own mutex, but that's not possible. using 1024 mutexes (more than one per worker, in the
   // default configuration) and hashed pathnames is pretty much the best we can do here.
   if (res < 0 && limitXAttrListLength && errno == ENODATA && !(flags & XATTR_REPLACE))
   {
      static std::array<Mutex, 1024> mutexHash;

      Mutex& mtx = mutexHash[std::hash<std::string>()(path) % mutexHash.size()];

      std::lock_guard<Mutex> lock(mtx);

      ssize_t listRes = ::listxattr(path.c_str(), NULL, 0);
      if (listRes < 0)
         res = listRes;
      else if (listRes + UserXAttrPrefix.size() + name.size() + 1 > XATTR_LIST_MAX)
         return FhgfsOpsErr_NOSPACE;

      res = setxattr(path.c_str(), (UserXAttrPrefix + name).c_str(), value, size, flags);
   }

   if (res == 0)
      return FhgfsOpsErr_SUCCESS;

   switch (errno)
   {
      case EEXIST:
         return FhgfsOpsErr_EXISTS;

      case ENODATA:
         return FhgfsOpsErr_NODATA;

      case ERANGE:
      case E2BIG:
         return FhgfsOpsErr_RANGE;

      case ENOENT:
         return FhgfsOpsErr_PATHNOTEXISTS;

      case ENOSPC:
         return FhgfsOpsErr_NOSPACE;

      default: // don't forward other errors to the client, but log them on the server
         LOG(GENERAL, ERR, "failed to set xattr", path, name, sysErr);
         return FhgfsOpsErr_INTERNAL;
   }
}

FhgfsOpsErr removeUserXAttr(const std::string& path, const std::string& name)
{
   int res = removexattr(path.c_str(), (UserXAttrPrefix + name).c_str());

   if (res == 0)
      return FhgfsOpsErr_SUCCESS;

   switch (errno)
   {
      case ENODATA:
         return FhgfsOpsErr_NODATA;

      case ERANGE:
      case E2BIG:
         return FhgfsOpsErr_RANGE;

      case ENOENT:
         return FhgfsOpsErr_PATHNOTEXISTS;

      default: // don't forward other errors to the client, but log them on the server
         LOG(GENERAL, ERR, "failed to remove xattr", path, name, sysErr);
         return FhgfsOpsErr_INTERNAL;
   }
}

std::pair<FhgfsOpsErr, std::vector<std::string>> listUserXAttrs(const std::string& path)
{
   auto listRes = listXAttrs(path);
   if (listRes.first == FhgfsOpsErr_SUCCESS)
      sanitizeForUser(listRes.second);

   return listRes;
}


}
