/**
 * Convert #defines or enums to strings
 *
 * See 'class NetMsgStrMapping' how to do an automatic mapping from defines (shell script)
 *
 */

#ifndef common_logging_DefineToStrMapping_h_gjKFhPeObdRJVP8ie4mbue
#define common_logging_DefineToStrMapping_h_gjKFhPeObdRJVP8ie4mbue

#include <map>
#include <string>

#include <common/toolkit/StringTk.h>

typedef std::map<int, std::string> DefineToStrMap;
typedef DefineToStrMap::iterator   DefineToStrMapIter;


class DefineToStrMapping
{
   protected:
      DefineToStrMap defineToStrMap;

   public:

      /**
       * Find the given define/enum in the map and return the corresponding string.
       * If it cannot be found,  there is likely a bug somewhere,
       * but we simply return the number as string then.
       */
      std::string defineToStr(int numericDefineValue)
      {
         DefineToStrMapIter iter = this->defineToStrMap.find(numericDefineValue);
         if (iter == this->defineToStrMap.end() )
         {
            // string not found, return number as string
            return StringTk::intToStr(numericDefineValue);
         }

         // return assigned string value
         return iter->second + " (" + StringTk::intToStr(numericDefineValue) + ")";
      }
};

#endif
