#ifndef OPEN_STRINGTK_H_
#define OPEN_STRINGTK_H_

#include <common/Common.h>
#include <common/toolkit/list/StrCpyList.h>


// manipulation
extern void StringTk_explode(const char* s, char delimiter, StrCpyList* outList);
static inline char* StringTk_strncpyTerminated(char* dest, const char* src, size_t count);

// numerical
static inline int StringTk_strToInt(const char* s);
static inline unsigned StringTk_strToUInt(const char* s);
static inline uint64_t StringTk_strToUInt64(const char* s);
extern bool StringTk_strToBool(const char* s);

// construction
static inline char* StringTk_strDup(const char* s);

static inline char* StringTk_subStr(const char* start, unsigned numChars);
extern char* StringTk_trimCopy(const char* s);
extern char* StringTk_kasprintf(const char *fmt, ...);

static inline char* StringTk_intToStr(int a);
static inline char* StringTk_uintToStr(unsigned a);

// inliners
static inline bool StringTk_hasLength(const char* s);



bool StringTk_hasLength(const char* s)
{
   return s[0] ? true : false;
}


char* StringTk_strncpyTerminated(char* dest, const char* src, size_t count)
{
   // Note: The problem with strncpy() is that dest is not guaranteed to be zero-terminated.
   // strlcpy() does guarantee that.
   // strlcpy() was removed in commit d26270061ae6 (string: Remove strlcpy()), use strscpy() instead.

   strscpy(dest, src, count);

   return dest;
}

int StringTk_strToInt(const char* s)
{
   return (int)simple_strtol(s, NULL, 10);
}

unsigned StringTk_strToUInt(const char* s)
{
   return (unsigned)simple_strtoul(s, NULL, 10);
}

uint64_t StringTk_strToUInt64(const char* s)
{
   return simple_strtoull(s, NULL, 10);
}

char* StringTk_strDup(const char* s)
{
   return kstrdup(s, GFP_NOFS);
}

/**
 * @return string is kmalloc'ed and has to be kfree'd by the caller
 */
char* StringTk_subStr(const char* start, unsigned numChars)
{
   char* subStr = (char*)os_kmalloc(numChars+1);

   if(numChars)
      memcpy(subStr, start, numChars);

   subStr[numChars] = 0;

   return subStr;
}

char* StringTk_intToStr(int a)
{
   char aStr[24];
   sprintf(aStr, "%d", a);

   return StringTk_strDup(aStr);
}

char* StringTk_uintToStr(unsigned a)
{
   char aStr[24];
   sprintf(aStr, "%u", a);

   return StringTk_strDup(aStr);
}

#endif /*OPEN_STRINGTK_H_*/
