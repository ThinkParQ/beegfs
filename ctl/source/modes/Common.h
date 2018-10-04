#ifndef CTL_COMMON_H
#define CTL_COMMON_H


#include <common/toolkit/NodesTk.h>
#include <vector>
#include <tuple>
#include <system_error>

namespace ctl {
namespace common {

enum class InfoDownloadErrorCode
{
   MgmtCommFailed=1,
   InfoDownloadFailed,
   ConfigurationError
};

const std::error_category& infoDownload_category();

inline std::error_code make_error_code(const InfoDownloadErrorCode condition)
{
   return std::error_code(static_cast<int>(condition), infoDownload_category());
}

nu::error_or<std::pair<NumNodeID, std::vector<std::shared_ptr<Node>>>>
downloadNodes(const NodeType nodeType);

nu::error_or<std::tuple<UInt16List,UInt16List,UInt16List>> downloadMirrorBuddyGroups(const NodeType nodeType);

}
}

namespace std {
template<> struct is_error_code_enum<ctl::common::InfoDownloadErrorCode> : public std::true_type{};
}

#endif
