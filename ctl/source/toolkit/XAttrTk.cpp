#include <common/toolkit/StringTk.h>
#include <common/system/System.h>
#include <linux/limits.h>
#include <sys/xattr.h>
#include "XAttrTk.h"

namespace
{
   /**
    * Create an instance of XAttrException.
    *
    * @param message   The main error message.
    * @param path      The path related to the error.
    * @param xattrName The name of the extended attribute related to the error.
    * @return an instance of XAttrException.
    */
   XAttrException createException(const std::string& message, const std::string& path,
      const std::string& xattrName)
   {
      int errorCode = errno;
      std::ostringstream stream;

      stream << message << " " << xattrName << " (" << path << "): " <<
         System::getErrString(errorCode) << std::endl;

      return XAttrException(stream.str());
   }
};

namespace XAttrTk {

/**
 * Set extended attributes of file or symbolic link path.
 *
 * @param path    The file or symbolic link path to get extended attributes set.
 * @param xattrs  A map of extended attributes names and their respective values.
 *                Each map object is a tuple containing the attribute value array
 *                and its length.
 * @throw XAttrException if setting extended attributes fails.
 */
void setXAttrs(const std::string& path, const XAttrMap& xattrs)
{
   for (auto xattr = xattrs.begin(); xattr != xattrs.end(); xattr++)
   {
      const char* xattrName = xattr->first.c_str();
      const char* xattrValue = &xattr->second.front();
      ssize_t xattrValueLength = xattr->second.size();

      if (lsetxattr(path.c_str(), xattrName, xattrValue, xattrValueLength, XATTR_CREATE))
      {
         throw createException("Failed to set extended attribute", path, xattrName);
      }
   }
}

/**
 * Get all extended attributes names and their respective values.
 *
 * @param path The file or symbolic link path to get extended attributes queried.
 * @return a map of extended attributes of the file to be copied.
 *         The map contains the attribute names and their respective values.
 *         Each value is a tuple containing the attribute value array and its length.
 * @throw XAttrException if getting extended attributes fails.
 */
XAttrMap getXAttrs(const std::string& path)
{
   XAttrMap attrs;
   std::vector<std::string> xattrNames = getXAttrNames(path);

   for (auto xattrName = xattrNames.begin(); xattrName != xattrNames.end(); xattrName++)
   {
      attrs[*xattrName] = getXAttrValue(path, *xattrName);
   }
   return attrs;
}

/**
 * Read the names of extended attributes of the file to be copied.
 *
 * @param path The file or symbolic link path to get extended attributes names queried.
 * @return a vector of names of extended attributes of the file to be copied.
 * @throw XAttrException if getting extended attributes names fails.
 */
std::vector<std::string> getXAttrNames(const std::string& path)
{
   XAttrValue names(XATTR_LIST_MAX);

   //Read list of extended attributes' names
   ssize_t length = llistxattr(path.c_str(), names.data(), names.size());

   if (length < 0)
   {
      throw createException("Failed to list extended attributes", path, "");
   }

   std::vector<std::string> xattrNames;
   StringTk::explode(std::string(names.data(), length), '\0', &xattrNames);

   return xattrNames;
}

/**
 * Read the value of an extended attribute of the file to be copied.
 *
 * @param path      The file or symbolic link path to get the extended attribute value queried.
 * @param xattrName The extended attribute name.
 * @return a vector containing the attribute value array.
 * @throw XAttrException if getting extended attributes values fails.
 */
XAttrValue getXAttrValue(const std::string& path, const std::string& xattrName)
{
   XAttrValue value(XATTR_SIZE_MAX, 0);

   //Read the extended attribute value
   ssize_t length = lgetxattr(path.c_str(), xattrName.c_str(), &value.front(), XATTR_SIZE_MAX);

   if (length < 0)
   {
      throw createException("Failed to read the value of the extended attribute", path, xattrName);
   }

   //Trim value
   value.resize(length);

   return value;
}

};
