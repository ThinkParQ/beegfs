#pragma once

#include <string>
#include <algorithm>

namespace EntryIdTk {

   bool isValidEntryIdFormat(const std::string& entryId);

   bool isValidHexToken(const std::string& token);

}

