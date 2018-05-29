#ifndef COMMON_TOOLKIT_ENTRYIDTK_H
#define COMMON_TOOLKIT_ENTRYIDTK_H

#include <string>
#include <algorithm>

namespace EntryIdTk {

   bool isValidEntryIdFormat(const std::string& entryId);

   bool isValidHexToken(const std::string& token);

}

#endif
