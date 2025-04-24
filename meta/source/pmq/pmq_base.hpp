#pragma once

#include <cassert>
#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdint.h>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <new>

// macro to align variables to cache line size
// There is C++ standardized value of std::hardware_destructive_interference_size.
// However that currently produces a warning, probably because of concerns about ABI stability.
// So instead I just hardcode a cache line size of 64 bytes for now.
// The worst that could happen would be bad performance.
//#define __pmq_cache_aligned alignas(std::hardware_destructive_interference_size)
#define __pmq_cache_aligned alignas(64)

// These #define's work for GCC and possibly other compilers.  To guarantee
// that these definitions are active wherever they could potentially work, I
// will define them unconditionally for now, instead of guarding them with
// #ifdef __GNUC__.
// TODO: try on more compilers and improve compatibility logic!

#if PMQ_WITH_PROFILING
#define __pmq_profiled __attribute__((noinline))  // could consider attribute "noipa" instead of "noinline"
#else
#define __pmq_profiled
#endif

// "artificial" is used for small inlined wrapper methods, such as operator[].
// In theory (and to some extent in practice) the effect should be that the
// code that gets inlined to a call site gets attributed to the _call site_
// instead of to the definition site of the inlined function -- reducing the
// effect of jumping around like wild files when debugging.

#define __pmq_artificial_method inline __attribute__((always_inline, artificial))
#define __pmq_artificial_func static inline __attribute__((always_inline, artificial))

// Attribute used for logging functions and other printf-style functions. If
// these functions are properly annotated, the compiler can check matching
// arguments in usage places.

#define __pmq_formatter(fmt_index, first_arg_index) \
   __attribute__((format(printf, (fmt_index), (first_arg_index))))

// treat format warnings as errors for the PMQ
// This could be a build system flag but for now I want the change just for
// this module in the larger system
#pragma GCC diagnostic error "-Wformat"


#ifdef NDEBUG
#define pmq_assert(expr)
#else
static inline void __pmq_assert_fail(const char *expr, const char *file, int line, const char *func)
{
   // this hopefully gives the logger a chance to save the logs.
   // If there was time, we should probably implement the logger in a separate component,
   // communicating using a shared memory mapping.

   sleep(3);
   __assert_fail(expr, file, line, func);
}
#define pmq_assert(expr) do { if (! (expr)) {  __pmq_assert_fail(#expr, __FILE__, __LINE__, __func__); } } while (0)
#endif



__pmq_artificial_func
void __pmq_assert_aligned(const void *ptr, size_t size)
{
   assert((uintptr_t) (ptr) % size == 0);
}

template<size_t size, typename T>
__pmq_artificial_func
T __attribute__((aligned(size))) *__pmq_assume_aligned(const T *ptr)
{
   __pmq_assert_aligned(ptr, size);
   return (T *) __builtin_assume_aligned(ptr, size);
}


static inline bool pmq_is_power_of_2(uint64_t value)
{
   assert(value != 0);
   return (value & (value - 1)) == 0;
}

static inline uint64_t pmq_mask_power_of_2(uint64_t value)
{
   assert(value != 0);
   assert((value & (value - 1)) == 0);
   return value - 1;
}


static inline constexpr uint64_t PMQ_Kilobytes(uint64_t count) { return count << 10; }
static inline constexpr uint64_t PMQ_Megabytes(uint64_t count) { return count << 20; }
static inline constexpr uint64_t PMQ_Gigabytes(uint64_t count) { return count << 30; }
static inline constexpr uint64_t PMQ_Terabytes(uint64_t count) { return count << 40; }
static inline constexpr uint64_t PMQ_Petabytes(uint64_t count) { return count << 50; }


/* Untyped slice class. This is mainly used for slice-copy operations, both for
 * memory and disk I/O. It saves some boilerplate and is a little bit safer to use.
 *
 * Note, we should check if we can replace this with a standard C++ type maybe.
 * But I personally don't consider this code a liability, and add
 * __pmq_artificial_method method improves the debugging experience.
 */
class Untyped_Slice
{
   void *m_data;
   size_t m_size;

public:
   __pmq_artificial_method void *data() const { return m_data; }
   __pmq_artificial_method size_t size() const { return m_size; }

   __pmq_artificial_method
   Untyped_Slice offset_bytes(size_t offset) const
   {
      assert(offset <= m_size);
      return Untyped_Slice((char *) m_data + offset, m_size - offset);
   }

   __pmq_artificial_method
   Untyped_Slice limit_size_bytes(size_t size) const
   {
      assert(size <= m_size);
      return Untyped_Slice(m_data, size);
   }

   __pmq_artificial_method
   Untyped_Slice sub_slice_bytes(size_t offset, size_t size) const
   {
      return offset_bytes(offset).limit_size_bytes(size);
   }

   __pmq_artificial_method
   Untyped_Slice()
   {
      m_data = nullptr;
      m_size = 0;
   }

   __pmq_artificial_method
      Untyped_Slice(void *data, size_t size)
   {
      m_data = data;
      m_size = size;
   }
};

__pmq_artificial_func
void zero_out_slice(Untyped_Slice dst)
{
   memset(dst.data(), 0, dst.size());
}

__pmq_artificial_func
void copy_slice(Untyped_Slice dst, Untyped_Slice src)
{
   assert(dst.size() == src.size());
   memcpy(dst.data(), src.data(), dst.size());
}

__pmq_artificial_func
void copy_slice_bytes(Untyped_Slice dst, Untyped_Slice src, size_t size_bytes)
{
   assert(size_bytes <= dst.size());
   assert(size_bytes <= src.size());
   memcpy(dst.data(), src.data(), size_bytes);
}

__pmq_artificial_func
void copy_to_slice(Untyped_Slice slice, const void *data, size_t size)
{
   assert(slice.size() >= size);
   memcpy(slice.data(), data, size);
}

__pmq_artificial_func
void copy_from_slice(void *data, Untyped_Slice slice, size_t size)
{
   assert(slice.size() >= size);
   memcpy(data, slice.data(), size);
}


/*
 * Typed slice type.
 *
 * Note, we should check if we can replace this using std::span (C++20).
 */
template<typename T>
class Slice
{
   T *m_data;
   size_t m_count;

public:

   __pmq_artificial_method
   T *data() const
   {
      return m_data;
   }

   __pmq_artificial_method
   size_t count() const
   {
      return m_count;
   }

   __pmq_artificial_method
   size_t size_in_bytes() const
   {
      return m_count * sizeof (T);
   }

   __pmq_artificial_method
   T get(size_t index) const
   {
      assert(index < m_count);
      return m_data[index];
   }

   __pmq_artificial_method
   T& at(size_t index)
   {
      assert(index < m_count);
      return m_data[index];
   }

   __pmq_artificial_method
   T const& at(size_t index) const
   {
      assert(index < m_count);
      return m_data[index];
   }

   __pmq_artificial_method
   Untyped_Slice untyped() const
   {
      return Untyped_Slice(m_data, m_count * sizeof (T));
   }

   __pmq_artificial_method
   Slice<T> slice_from(size_t start_index)
   {
      assert(start_index <= m_count);
      return Slice<T>(m_data + start_index, m_count - start_index);
   }

   __pmq_artificial_method
   Slice<T> slice_to(size_t count)
   {
      assert(count <= m_count);
      return Slice<T>(m_data, count);
   }

   __pmq_artificial_method
   Slice<T> sub_slice(size_t start_index, size_t count)
   {
      return slice_from(start_index).slice_to(count);
   }

   __pmq_artificial_method
   Slice()
   {
      m_data = nullptr;
      m_count = 0;
   }

   __pmq_artificial_method
   Slice(T *data, size_t count)
   {
      m_data = data;
      m_count = count;
   }
};

template<typename T>
__pmq_artificial_func
void copy_to_slice(Slice<T> slice, const void *data, size_t size)
{
   assert(slice.size_in_bytes() >= size);
   memcpy(slice.data(), data, size);
}

template<typename T>
__pmq_artificial_func
void copy_from_slice(void *data, Slice<T> slice, size_t size)
{
   assert(slice.size_in_bytes() >= size);
   memcpy(data, slice.data(), size);
}



// A reference type, which wraps a bare pointer. The semantics are the same as
// pointer but we don't allow indexing. In other words, the point of this class
// is to make clear that it doesn't point to an array but only to a single
// (potentially null) object.
// In contrast to C++ reference types (T& value), no surprises given value
// syntax but pointer semantics.

template<typename T>
class Pointer
{
   T *m_ptr;

public:

   __pmq_artificial_method
   T *ptr() const
   {
      return m_ptr;
   }

   __pmq_artificial_method
   const T *const_ptr() const
   {
      return m_ptr;
   }

   __pmq_artificial_method
   Pointer<const T> as_const() const
   {
      return Pointer<const T>(m_ptr);
   }

   __pmq_artificial_method
   T *operator->()
   {
      return m_ptr;
   }

   __pmq_artificial_method
   Pointer(T *ptr)
   {
      assert(ptr);
      m_ptr = ptr;
   }
};
