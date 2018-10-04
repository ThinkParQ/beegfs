#ifndef STRINGTK_H_
#define STRINGTK_H_

#include <common/Common.h>
#include <common/storage/StorageErrors.h>
#include <common/toolkit/BufferTk.h>

#include <iterator>

#define STRINGTK_STRING_NOT_FOUND_RET (std::string::npos)
#define STRINGTK_ID_SEPARATOR "-"


class StringTk
{
   public:
      // manipulation
      static std::string trim(const std::string s);
      static void explode(const std::string s, char delimiter, StringList* outList);
      static void explode(const std::string s, char delimiter, StringVector* outVec);
      static void explodeEx(const std::string s, char delimiter, bool useTrim, StringList* outList);
      static void explodeEx(const std::string s, char delimiter, bool useTrim, StringVector* outVec);
      static std::string implode(char delimiter, StringList& inList, bool useTrim);

      // transformation
      static int strToInt(const char* s);
      static unsigned strToUInt(const char* s);
      static unsigned strHexToUInt(const char* s);
      static unsigned strOctalToUInt(const char* s);
      static int64_t strToInt64(const char* s);
      static uint64_t strToUInt64(const char* s);
      static bool strToBool(const char* s);
      static std::string intToStr(int a);
      static std::string uintToStr(unsigned a);
      static std::string uintToHexStr(unsigned a);
      static std::string uint16ToHexStr(uint16_t a);
      static std::string int64ToStr(int64_t a);
      static std::string uint64ToHexStr(uint64_t a);
      static std::string uint64ToStr(uint64_t a);
      static std::string doubleToStr(double a);
      static std::string doubleToStr(double a, int precision);
      static std::string timespanToStr(int64_t timespan_seconds);

      static std::string uint16VecToStr(const UInt16Vector* vec);
      static void strToUint16Vec(std::string& s, UInt16Vector* outVec);

      // inspection
      static bool isNumeric(const std::string testString);
      static void genRandomAlphaNumericString(std::string& inOutString, const int length);

   private:
      StringTk() {}

   public:
      // inliners

      static int strToInt(const std::string s)
      {
         return strToInt(s.c_str() );
      }

      static unsigned strToUInt(const std::string s)
      {
         return strToUInt(s.c_str() );
      }

      static unsigned strOctalToUInt(const std::string s)
      {
         return strOctalToUInt(s.c_str() );
      }

      static unsigned strHexToUInt(const std::string s)
      {
         return strHexToUInt(s.c_str() );
      }

      static int64_t strToInt64(const std::string s)
      {
         return strToInt64(s.c_str() );
      }

      static bool strToBool(const std::string s)
      {
         return strToBool(s.c_str() );
      }

      static double strToDouble(const std::string s)
      {
         return atof(s.c_str() );
      }

      static char* strncpyTerminated(char* dest, const char* src, size_t count)
      {
         // Note: The problem with strncpy() is that dest is not guaranteed to be zero-terminated.

         size_t srcLen = strlen(src);

         if(likely(count) )
         {
            size_t copyLen = (srcLen >= count) ? (count - 1) : srcLen;

            memcpy(dest, src, copyLen);
            dest[copyLen] = '\0';
         }

         return dest;
      }

      /**
       * Calculate the hash of the string.
       *
       * @param buf    - string to caculate the hash for
       * @param bufLen - length of buf
       */
      static unsigned strChecksum(const char* buf, int bufLen)
      {
         // Note: We use a 32bit checksum here to get the same result (fast) on x86 and x86_64

         return BufferTk::hash32(buf, bufLen);
      }


      /**
       * Get the time-stamp part from an EntryID
       */
      static FhgfsOpsErr timeStampFromEntryID(const std::string& inEntryID, std::string &outEntryID)
      {
         size_t firstSepPos = inEntryID.find(STRINGTK_ID_SEPARATOR);
         if (firstSepPos == (size_t) STRINGTK_STRING_NOT_FOUND_RET)
         {  // special entryID, such as 'root' and 'lost+found', we return it as it is
            outEntryID = inEntryID;
            return FhgfsOpsErr_SUCCESS;
         }

         size_t secondSepPos = inEntryID.find(STRINGTK_ID_SEPARATOR, firstSepPos + 1);
         if (secondSepPos == (size_t) STRINGTK_STRING_NOT_FOUND_RET)
         {  // That should not happen, return it as it is, but the caller should log an error
            outEntryID = inEntryID;
            return FhgfsOpsErr_INTERNAL;
         }

         // +1 and -1 to remove the separator '-'
         outEntryID = inEntryID.substr(firstSepPos + 1, secondSepPos - firstSepPos - 1);

         return FhgfsOpsErr_SUCCESS;
      }

      template<typename T>
      static std::string implode(const char* const delimiter, const std::vector<T>& inVec)
      {
         size_t numElements = inVec.size();

         if (numElements == 0)
            return "";
         else
         {
            std::ostringstream os;

            if (numElements == 1)
            {
               os << inVec[0];
            }
            else
            {
               std::copy(inVec.begin(), inVec.end() - 1,
                  std::ostream_iterator<T>(os, delimiter));
               os << *inVec.rbegin();
            }
            return os.str();
         }
      }
      
      /*
       * creates a delimiter-seperated list from an input container of elements and sptils lines if
       * longer than maxLineLength
       *
       * @param delimiter a char to be used as delimiter
       * @param elements the input container
       * @param maxLineLength
       * @return a vector of string representing lines (at least one element with an empty line is
       * contained if input was empty)
       */
      template<typename Container>
      static StringVector implodeMultiLine(const Container& elements, char delimiter,
                                           unsigned maxLineLength)
      {
         StringVector result;
         std::string line;
         
         // add first element
         auto iter = elements.begin();
         if (iter != elements.end())
         {
            std::stringstream elementStr;
            elementStr << *iter;
            line += elementStr.str();
            ++iter;
         }
         
         for ( ; iter != elements.end();  iter++)
         {
            line += delimiter;
            
            std::stringstream elementStr;
            elementStr << *iter;
            
            if (line.length() + elementStr.tellp() + 1 > maxLineLength)
            {
               result.push_back(line);
               line.clear();
            }
            
            line += elementStr.str();
         }
         
         result.push_back(line);
         return result;
      }
};

#endif /*STRINGTK_H_*/
