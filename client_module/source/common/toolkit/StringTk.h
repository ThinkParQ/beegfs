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
static inline int64_t StringTk_strToInt64(const char* s);
static inline uint64_t StringTk_strToUInt64(const char* s);
extern bool StringTk_strToBool(const char* s);

// construction
static inline char* StringTk_strDup(const char* s);

static inline char* StringTk_strnDup(const char* s, size_t len);

static inline char* StringTk_subStr(const char* start, unsigned numChars);
extern char* StringTk_trimCopy(const char* s);
extern char* StringTk_kasprintf(const char *fmt, ...);

static inline char* StringTk_intToStr(int a);
static inline char* StringTk_uintToStr(unsigned a);
static inline char* StringTk_uintToHexStr(unsigned a);
static inline char* StringTk_int64ToStr(int64_t a);
static inline char* StringTk_uint64ToHexStr(uint64_t a);

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

   strlcpy(dest, src, count);

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

int64_t StringTk_strToInt64(const char* s)
{
   // note: other than the unsigned version, simple_strtoll is not exported in some kernel versions
   // and hence we use an emulation here

   if(*s == '-')
      return -StringTk_strToUInt64(&s[1] );

   return StringTk_strToUInt64(s);
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
 * kstrndup wrapper,
 *
 * Note: Be careful on using it, is only supported since 2.6.23, kstrdup() is used for
 * older kernel versions.
 */
char* StringTk_strnDup(const char* s, size_t len)
{
#ifndef KERNEL_HAS_KSTRNDUP
   return kstrdup(s, GFP_NOFS);
#else
   return kstrndup(s, len, GFP_NOFS);
#endif // LINUX_VERSION_CODE

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

char* StringTk_uintToHexStr(unsigned a)
{
   char aStr[24];
   sprintf(aStr, "%X", a);

   return StringTk_strDup(aStr);
}

char* StringTk_int64ToStr(int64_t a)
{
   char aStr[24];
   sprintf(aStr, "%lld", (long long)a);

   return StringTk_strDup(aStr);
}

char* StringTk_uint64ToHexStr(uint64_t a)
{
   char aStr[24];
   sprintf(aStr, "%llX", (unsigned long long)a);

   return StringTk_strDup(aStr);
}

#endif /*OPEN_STRINGTK_H_*/
