#ifndef META_XATTRTK_H
#define META_XATTRTK_H

#include <common/storage/StorageErrors.h>

#include <utility>

namespace XAttrTk
{
   extern const std::string UserXAttrPrefix;

   std::pair<FhgfsOpsErr, std::vector<std::string>> listXAttrs(const std::string& path);

   std::tuple<FhgfsOpsErr, std::vector<char>, ssize_t> getXAttr(const std::string& path,
      const std::string& name, size_t maxSize);

   void sanitizeForUser(std::vector<std::string>& names);

   void removeMetadataAttrs(std::vector<std::string>& names);

   FhgfsOpsErr setUserXAttr(const std::string& path, const std::string& name, const void* value,
      size_t size, int flags);

   FhgfsOpsErr removeUserXAttr(const std::string& path, const std::string& name);

   inline std::tuple<FhgfsOpsErr, std::vector<char>, ssize_t> getUserXAttr(const std::string& path,
      const std::string& name, size_t maxSize)
   {
      return getXAttr(path, UserXAttrPrefix + name, maxSize);
   }

   std::pair<FhgfsOpsErr, std::vector<std::string>> listUserXAttrs(const std::string& path);
}

#endif
