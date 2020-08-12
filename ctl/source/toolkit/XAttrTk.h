#pragma once

#include <common/Common.h>

typedef std::vector<char> XAttrValue;
typedef std::map<std::string, XAttrValue> XAttrMap;

DECLARE_NAMEDEXCEPTION(XAttrException, "XAttrException")

namespace XAttrTk
{
  //! \note Does _not_ offer strong exception guarantee: xattrs may
  //! end up partially set if setting one of them fails.
  void setXAttrs(const std::string& path, const XAttrMap& xattrs);
  XAttrMap getXAttrs(const std::string& path);

  std::vector<std::string> getXAttrNames(const std::string& path);
  XAttrValue getXAttrValue(const std::string& path, const std::string& xattrName);
};
