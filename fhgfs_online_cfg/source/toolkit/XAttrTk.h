#ifndef XATTRTK_H_
#define XATTRTK_H_

#include <common/Common.h>

#include <unordered_map>
#include <attr/xattr.h>

typedef std::vector<char> XAttrValue;
typedef std::map<std::string, XAttrValue> XAttrMap;

DECLARE_NAMEDEXCEPTION(XAttrException, "XAttrException")

/**
 * Extended attributes toolkit.
 */
class XAttrTk
{
   public:
      virtual ~XAttrTk()
      {
      }

      static void setXAttrs(const std::string& path, const XAttrMap& xattrs);

      static void getXAttrs(const std::string& path, XAttrMap& outXAttrs);

      static std::vector<std::string> getXAttrNames(const std::string& path);

      static XAttrValue getXAttrValue(const std::string& path, const std::string& xattrName);

   private:

      XAttrTk()
      {
      }

      static XAttrException createException(const std::string& message,
         const std::string& path, const std::string& xattrName = "");
};

#endif /* XATTRTK_H_ */
