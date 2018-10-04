#include "EntryIdTk.h"

namespace EntryIdTk {

bool isValidEntryIdFormat(const std::string& entryId)
{

   const auto sep1 = entryId.find('-');
   const auto sep2 = entryId.find('-', sep1 + 1);

   return  sep1 != std::string::npos && sep2 != std::string::npos
          &&  isValidHexToken(entryId.substr(0, sep1))
          &&  isValidHexToken(entryId.substr(sep1 + 1, sep2 - (sep1 + 1)))
          &&  isValidHexToken(entryId.substr(sep2 + 1));
}

bool isValidHexToken(const std::string& token) {
   const auto notUpperHexDigit = [] (const char c) {
      return  !std::isxdigit(c) || std::islower(c);
   };
   const auto isHex = [&notUpperHexDigit] (const std::string& s) {
      return std::count_if(s.begin(), s.end(), notUpperHexDigit) == 0;
   };

   return !token.empty() && (token.size() <= 8) && isHex(token);
}

}
