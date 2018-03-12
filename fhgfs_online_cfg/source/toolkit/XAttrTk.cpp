#include <common/toolkit/StringTk.h>
#include <common/system/System.h>
#include <linux/limits.h>
#include <attr/xattr.h>
#include "XAttrTk.h"

/**
 * Sets extended attributes of file or symbolic link path.
 *
 * @param path    The file or symbolic link path to get extended attributes set.
 * @param xattrs  A map of extended attributes names and their respective values.
 *                Each map object is a tuple containing the attribute value array
 *                and its length.
 * @throw XAttrException if setting extended attributes fails.
 */
void XAttrTk::setXAttrs(const std::string& path, const XAttrMap& xattrs)
{
   for (auto xattr = xattrs.begin(); xattr != xattrs.end(); xattr++)
   {
      const char* xattrName = xattr->first.c_str();
      const char* xattrValue = &xattr->second.front();
      ssize_t xattrValueLength = xattr->second.size();

      int result = lsetxattr(path.c_str(), xattrName, xattrValue, xattrValueLength, XATTR_CREATE);

      if (result < 0)
      {
         throw createException("Failed to set extended attribute", xattr->first);
      }
   }
}

/**
 * Get all extended attributes names and their respective values.
 *
 * @param path      The file or symbolic link path to get extended attributes queried.
 * @param outXAttrs The output map of extended attributes of the file to be copied.
 *                  The map contains the attribute names and their respective values.
 *                  Each value is a tuple containing the attribute value array and its length.
 * @return true if the map could be retrieved; false otherwise.
 * @throw XAttrException if getting extended attributes fails.
 */
void XAttrTk::getXAttrs(const std::string& path, XAttrMap& outXAttrs)
{
   std::vector<std::string> xattrNames = getXAttrNames(path);

   for (auto xattrName = xattrNames.begin(); xattrName != xattrNames.end(); xattrName++)
   {
      outXAttrs[*xattrName] = getXAttrValue(path, *xattrName);
   }
}

/**
 * Read the names of extended attributes of the file to be copied.
 *
 * @param path The file or symbolic link path to get extended attributes names queried.
 * @return a vector of names of extended attributes of the file to be copied.
 * @throw XAttrException if getting extended attributes names fails.
 */
std::vector<std::string> XAttrTk::getXAttrNames(const std::string& path)
{
   std::vector<std::string> xattrNames;
   XAttrValue names(XATTR_LIST_MAX);

   //Read the list of attributes names
   ssize_t length = llistxattr(path.c_str(), &names.front(), XATTR_LIST_MAX);

   if (length > 0)
   {
      //Trim value
      names.resize(length);

      StringTk::explode(std::string(&names.front(), length), '\0', &xattrNames);
   }

   if (length < 0)
   {
      throw createException("Failed to list extended attributes", path);
   }

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
XAttrValue XAttrTk::getXAttrValue(const std::string& path, const std::string& xattrName)
{
   XAttrValue xattrValue(XATTR_SIZE_MAX, 0);

   //Read the extended attribute value
   ssize_t length = lgetxattr(path.c_str(), xattrName.c_str(), &xattrValue.front(), XATTR_SIZE_MAX);

   if (length < 0)
   {
      throw createException("Failed to read the value of the extended attribute", path, xattrName);
   }

   //Trim value
   xattrValue.resize(length);

   return xattrValue;
}

/**
 * Create an instance of XAttrException.
 *
 * @param message   The main error message.
 * @param path      The path related to the error.
 * @param xattrName The name of the extended attribute related to the error.
 * @return an instance of XAttrException.
 */
XAttrException XAttrTk::createException(const std::string& message, const std::string& path,
   const std::string& xattrName)
{
   int errcode = errno;
   std::ostringstream stream;

   stream << message << " " << xattrName << " (" << path << "): " <<
      System::getErrString(errcode) << std::endl;

   return XAttrException(stream.str());
}
