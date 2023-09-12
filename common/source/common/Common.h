#ifndef COMMON_H_
#define COMMON_H_

// define certain macros for (u)int64_t in inttypes.h; this is useful for printf on 32-/64-bit
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif // __STDC_FORMAT_MACROS

#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <inttypes.h>
#include <iostream>
#include <iomanip> // output formating
#include <fstream>
#include <sstream>
#include <pthread.h>
#include <map>
#include <set>
#include <list>
#include <vector>
#include <stdexcept>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <cerrno>
#include <unistd.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <sys/poll.h>
#include <netdb.h>
#include <algorithm>
#include <sys/time.h>
#include <fcntl.h>
#include <boost/smart_ptr/make_unique.hpp>
#include <memory>

/**
 * NOTE: These timeouts can now be overridden by the connMessagingTimeouts
 * option in the configuration file. If that option is unset or set to <=0, we
 * still default to these constants.
 */
#define CONN_LONG_TIMEOUT     600000
#define CONN_MEDIUM_TIMEOUT    90000
#define CONN_SHORT_TIMEOUT     30000

typedef std::map<std::string, std::string> StringMap;
typedef StringMap::iterator StringMapIter;
typedef StringMap::const_iterator StringMapCIter;
typedef StringMap::value_type StringMapVal;

typedef std::map<std::string, int64_t> StringInt64Map;
typedef StringInt64Map::iterator StringInt64MapIter;
typedef StringInt64Map::const_iterator StringInt64MapCIter;
typedef StringInt64Map::value_type StringInt64MapVal;

typedef std::set<std::string> StringSet;
typedef StringSet::iterator StringSetIter;
typedef StringSet::const_iterator StringSetCIter;

typedef std::list<std::string> StringList;
typedef StringList::iterator StringListIter;
typedef StringList::const_iterator StringListConstIter;

typedef std::vector<std::string> StringVector;
typedef StringVector::iterator StringVectorIter;
typedef StringVector::const_iterator StringVectorConstIter;

typedef std::list<uint8_t> UInt8List;
typedef UInt8List::iterator UInt8ListIter;
typedef UInt8List::const_iterator UInt8ListConstIter;

typedef std::set<uint16_t> UInt16Set;
typedef UInt16Set::iterator UInt16SetIter;
typedef UInt16Set::const_iterator UInt16SetCIter;

typedef std::list<uint16_t> UInt16List;
typedef UInt16List::iterator UInt16ListIter;
typedef UInt16List::const_iterator UInt16ListConstIter;

typedef std::vector<uint16_t> UInt16Vector;
typedef UInt16Vector::iterator UInt16VectorIter;
typedef UInt16Vector::const_iterator UInt16VectorConstIter;

typedef std::vector<UInt16Set> UInt16SetVector;
typedef UInt16SetVector::iterator UInt16SetVectorIter;
typedef UInt16SetVector::const_iterator UInt16SetVectorConstIter;

typedef std::vector<UInt16List> UInt16ListVector;

typedef std::set<int> IntSet;
typedef IntSet::iterator IntSetIter;
typedef IntSet::const_iterator IntSetCIter;

typedef std::set<unsigned> UIntSet;
typedef UIntSet::iterator UIntSetIter;
typedef UIntSet::const_iterator UIntSetCIter;

typedef std::list<int> IntList;
typedef IntList::iterator IntListIter;
typedef IntList::const_iterator IntListConstIter;

typedef std::list<unsigned> UIntList;
typedef UIntList::iterator UIntListIter;
typedef UIntList::const_iterator UIntListConstIter;

typedef std::vector<unsigned> UIntVector;
typedef UIntVector::iterator UIntVectorIter;
typedef UIntVector::const_iterator UIntVectorConstIter;

typedef std::vector<int> IntVector;
typedef IntVector::iterator IntVectorIter;
typedef IntVector::const_iterator IntVectorConstIter;

typedef std::list<short> ShortList;
typedef ShortList::iterator ShortListIter;
typedef ShortList::const_iterator ShortListConstIter;

typedef std::list<unsigned short> UShortList;
typedef UShortList::iterator UShortListIter;
typedef UShortList::const_iterator UShortListConstIter;

typedef std::vector<short> ShortVector;
typedef ShortVector::iterator ShortVectorIter;
typedef ShortVector::const_iterator ShortVectorConstIter;

typedef std::vector<unsigned short> UShortVector;
typedef UShortVector::iterator UShortVectorIter;
typedef UShortVector::const_iterator UShortVectorConstIter;

typedef std::vector<char> CharVector;
typedef CharVector::iterator CharVectorIter;
typedef CharVector::const_iterator CharVectorConstIter;

typedef std::list<int64_t> Int64List;
typedef Int64List::iterator Int64ListIter;
typedef Int64List::const_iterator Int64ListConstIter;

typedef std::list<uint64_t> UInt64List;
typedef UInt64List::iterator UInt64ListIter;
typedef UInt64List::const_iterator UInt64ListConstIter;

typedef std::vector<int64_t> Int64Vector;
typedef Int64Vector::iterator Int64VectorIter;
typedef Int64Vector::const_iterator Int64VectorConstIter;

typedef std::vector<uint64_t> UInt64Vector;
typedef UInt64Vector::iterator UInt64VectorIter;
typedef UInt64Vector::const_iterator UInt64VectorConstIter;

typedef std::list<bool> BoolList;
typedef BoolList::iterator BoolListIter;
typedef BoolList::const_iterator BoolListConstIter;

#define BEEGFS_MIN(a, b) \
   ( ( (a) < (b) ) ? (a) : (b) )
#define BEEGFS_MAX(a, b) \
   ( ( (a) < (b) ) ? (b) : (a) )


#define SAFE_DELETE(p) \
   do{ if(p) {delete (p); (p)=NULL;} } while(0)
#define SAFE_FREE(p) \
   do{ if(p) {free(p); (p)=NULL;} } while(0)

#define SAFE_DELETE_NOSET(p) \
   do{ if(p) delete (p); } while(0)
#define SAFE_FREE_NOSET(p) \
   do{ if(p) free(p); } while(0)

// typically used for optional out-args
#define SAFE_ASSIGN(destPointer, sourceValue) \
   do{ if(destPointer) {*(destPointer) = (sourceValue);} } while(0)


// gcc branch optimization hints
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x)  __builtin_expect(!!(x), 0)


// this macro mutes warnings about unused variables
#define IGNORE_UNUSED_VARIABLE(a)   do{ if( ((long)a)==1) {} } while(0)

// this macro mutes warnings about unsused variables that are only used in debug build
#ifdef BEEGFS_DEBUG
#define IGNORE_UNUSED_DEBUG_VARIABLE(a)   do{ /* do nothing */ } while(0)
#else
#define IGNORE_UNUSED_DEBUG_VARIABLE(a)   do{ if( ((long)a)==1) {} } while(0)
#endif


// dumps stack in case of a buggy condition
#define BEEGFS_BUG_ON(condition, msgStr) \
   do { \
      if(unlikely(condition) ) { \
         fprintf(stderr, "%s:%d: %s\n", __func__, __LINE__, msgStr); \
         LogContext(std::string(__func__) + StringTk::intToStr(__LINE__) ).logErr( \
            std::string("BUG: ") + msgStr + std::string(" (dumping stack...)") ); \
         LogContext(std::string(__func__) + StringTk::intToStr(__LINE__) ).logBacktrace(); \
      } \
   } while(0)


#ifdef LOG_DEBUG_MESSAGES
   #define BEEGFS_BUG_ON_DEBUG(condition, msgStr) BEEGFS_BUG_ON(condition, msgStr)
#else // !LOG_DEBUG_MESSAGES
   #define BEEGFS_BUG_ON_DEBUG(condition, msgStr) \
      do { /* nothing */ } while(0)
#endif // LOG_DEBUG_MESSAGES

//logtype to enable syslog
enum LogType
{
    LogType_SYSLOG=0,
    LogType_LOGFILE
};

#ifdef BEEGFS_DEBUG
   #define BEEGFS_DEBUG_PROFILING
#endif

// defined `override` (from c++11) if the compiler does not provide it. gcc >= 4.7 has it,
// clang since 3.0.
// we don't support gcc <= 4.4, so don't check those.
#if (!__clang__ && __GNUC__ == 4 && __GNUC_MINOR__ < 7) || (__clang__ && __clang_major__ < 3)
   #define override
#endif

// gcc 4.4 does not seem to support eq-operators for enum classes. other old gcc versions may have
// this problem.
#if (!__clang__ && __GNUC__ == 4 && __GNUC_MINOR__ == 4 && __GNUC_PATCHLEVEL__ == 0)
# define GCC_COMPAT_ENUM_CLASS_OPEQNEQ(TYPE) \
   inline bool operator==(TYPE a, TYPE b) { return memcmp(&a, &b, sizeof(a)) == 0; } \
   inline bool operator!=(TYPE a, TYPE b) { return !(a == b); }
#else
# define GCC_COMPAT_ENUM_CLASS_OPEQNEQ(TYPE)
#endif

// define a nullptr type and value (also from c++11) if the compiler does not provide it.
// gcc >= 4.6 has it, as does clang >= 3.0.
// this merely approximates the actual nullptr behaviour from c++11, but it approximates it close
// enough to be useful.
// the implementation is taken from N2431, which proposes nullptr as a language feature.
#if (!__clang__ && __GNUC__ == 4 && __GNUC_MINOR__ < 6) || (__clang__ && __clang_major__ < 3)
struct nullptr_t
{
   template<typename T>
   operator T*() const { return 0; }

   template<typename C, typename T>
   operator T C::*() const { return 0; }

   template<typename T>
   operator std::unique_ptr<T>() const { return {}; }

   template<typename T, typename Deleter>
   operator std::unique_ptr<T, Deleter>() const { return {}; }

   template<typename T>
   operator std::shared_ptr<T>() const { return {}; }

   template<typename T>
   operator std::weak_ptr<T>() const { return {}; }

   void operator&() = delete;

   // add a bool-ish conversion operator to allow `if (nullptr)` (e.g. in templates)
   typedef void (nullptr_t::*bool_type)();
   operator bool_type() const { return 0; }
};

#define nullptr ((const nullptr_t)nullptr_t{})
#endif

#if (!__clang__ && __GNUC__ == 4 && __GNUC_MINOR__ < 6) || (__clang__ && __clang_major__ < 3)
# define noexcept(...)
#endif

// libstdc++ <= 4.6 calls std::chrono::steady_clock monotonic_clock, set alias
#if __GLIBCXX__ && __GLIBCXX__ < 20120322
namespace std
{
namespace chrono
{
typedef monotonic_clock steady_clock;
}
}
#endif

/*
 * Optional definitions:
 * - BEEGFS_NO_LICENSE_CHECK: Disables all license checking at startup and runtime
 */

/*
 * Debug definitions:
 * - BEEGFS_DEBUG: Generally defined for debug builds. Might turn on some other debug definitions
 *    below automatically.
 *
 * - LOG_DEBUG_MESSAGES: Enables logging of some extra debug messages that will not be
 *    available otherwise
 * - DEBUG_REFCOUNT: Enables debugging of ObjectReferencer::refCount. Error messages will
 *    be logged if refCount is less than zero
 * - DEBUG_MUTEX_LOCKING: Enables the debug functionality of the SafeRWLock class
 * - BEEGFS_DEBUG_PROFILING: Additional timestamps in log messages.
 */

#ifdef BEEGFS_DEBUG
namespace beegfs_debug
{
__attribute__((noreturn))
extern void assertMsg(const char* file, unsigned line, const char* condition);
}

# define ASSERT(condition) \
   do { \
      if (!(condition)) \
         ::beegfs_debug::assertMsg(__FILE__, __LINE__, #condition); \
   } while (0)
#else
# define ASSERT(cond) do {} while (0)
#endif

#if defined (__GLIBC__) && (__GLIBC__ >= 2) && (__GLIBC_MINOR__ >= 24)
#define USE_READDIR_R 0
#else
#define USE_READDIR_R 1
#endif

#if __GNUC__ > 6
# define BEEGFS_FALLTHROUGH [[fallthrough]]
#else
# define BEEGFS_FALLTHROUGH
#endif

// XXX not sure about this. C++17 is the requirement for [[nodiscard]]
#define BEEGFS_NODISCARD [[nodiscard]]

// version number of both the network protocol and the on-disk data structures that are versioned.
// must be kept in sync with client.
#define BEEGFS_DATA_VERSION (uint32_t(0))

#endif /*COMMON_H_*/
