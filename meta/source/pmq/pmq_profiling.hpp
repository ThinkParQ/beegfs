#pragma once

#include <unistd.h>
#include <sys/syscall.h>

#include <mutex>
#include <condition_variable>

#if PMQ_WITH_PROFILING

// NOTE: this requires building with matching headers in the include-path for
// tracy. Tracy is a "frame profiler": https://github.com/wolfpld/tracy
// If PMQ is built as part of a bigger project (e.g. BeeGFS metadata server)
// buildin with the proper settings may not be supported (yet).
// A build setup that supports tracy currently exists as part of the flex-docs
// repository (ask the BeeGFS team).

#  include <Tracy.hpp>

#  define PMQ_PROFILING_CTX FrameMark
#  define PMQ_PROFILED_SCOPE(name) ZoneScopedN(name)
#  define PMQ_PROFILED_FUNCTION ZoneScoped
#  define PMQ_PROFILED_MUTEX(name) TracyLockable(std::mutex, name)
#  define PMQ_PROFILED_CONDVAR(name) std::condition_variable_any name
#  define PMQ_PROFILED_LOCK(name, themutex) \
      auto& __ref__##name(themutex); \
      std::lock_guard<LockableBase(std::mutex)> name(__ref__##name); \
      LockMark(__ref__##name)
#  define PMQ_PROFILED_UNIQUE_LOCK(name, themutex) \
      auto& __ref__##name(themutex); \
      std::unique_lock<LockableBase(std::mutex)> name(__ref__##name); \
      LockMark(__ref__##name) \

#else

#  define PMQ_PROFILING_CTX
#  define PMQ_PROFILED_SCOPE(name)
#  define PMQ_PROFILED_FUNCTION
#  define PMQ_PROFILED_MUTEX(name) std::mutex name
#  define PMQ_PROFILED_CONDVAR(name) std::condition_variable name
#  define PMQ_PROFILED_LOCK(name, themutex) \
      std::lock_guard<std::mutex> name(themutex)
#  define PMQ_PROFILED_UNIQUE_LOCK(name, themutex) \
      std::unique_lock<std::mutex> name(themutex)

#endif
