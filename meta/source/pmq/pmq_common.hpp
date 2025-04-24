#pragma once

#include <new>  // std::bad_alloc

#include "pmq_base.hpp"
#include "pmq_logging.hpp"
#include "pmq_posix_io.hpp"
#include "pmq_profiling.hpp"

#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <dirent.h>

//
// Simple allocating slice class with delayed allocation.
//
// Why don't I just use std::vector or similar? After all, std::vector is a
// well known standard solution that allocates a contiguous buffer of memory.
//
// I understand this concern, but I've written several simple classes anyway.
// Let me try and defend this case of "NIH". (It may or may not convince the
// reader).
//
// This code is much more straightforward and simple compared to STL headers.
// It is basically "new" and "delete" wrapped in a simple package together with
// operator[] and a way to get a slice to the memory without attached lifetime
// semantics.
//
// Most of the STL classes try to be very generic solutions applicable in a
// wide variety of use cases. While using with standardized solutions has the
// advantages of familiarity, this flexibility and wide applicability comes
// with a complexity cost that brings a disadvantage to anyone working with the
// codebase.
//
// Beyond having a fill level separate from allocation size (size() vs
// capacity()), std::vector has all sorts of methods and functionality to
// support pushing, popping, emplacing, iterators, constructors, destructors,
// and so on. It is highly flexible, which shows whenever an actual
// instanciated vector type is printed on the terminal, including template
// allocator parameter amongst to other things.
//
// All this is ill-fitting for our simple use case. For a queue we just need a
// few preallocated buffers. Just for convenience and to get a little safety,
// the Alloc_Slice wrapper class was created -- so we can do bounds checking
// and get to automatically deallocate the buffers in the destructor.
//
// The size() field and associated semantics that come with std::vector are
// baggage that we can't make use of (we have multiple cursors that wrap around
// our buffers in circular fashion). These semantics are not just available,
// but are understood by programmers as how std::vector gets used.
//
// From a mere functionality standpoint this shouldn't be an issue -- We could
// make sure that we call .resize(N) only once in the beginning and never call
// e.g. push_back(), emplace_back(), reserve(), or similar. This way we'd
// essentially be considering the size() as a constant i.e. ignore it.
//
// However, again, this usage of the type is not guaranteed.  The sight of a
// std::vector normally suggest pushing (maybe popping), resizing and
// reserving, buffer reallocation, pointer/iterator invalidation, and runtime
// exceptions.
//
// With the Alloc_Slice class on the other hand, there is no reallocation and
// consequently no iterator invalidation.  Exceptions might or might not happen
// depending on compile settings -- but only at construction time, i.e. program
// startup. Because no reallocations are possible, no pointer invalidation /
// iterator invalidation is possible.
//
// Compared to std::vector and other STL headers, significantly less header
// code gets included, so the code compiles quicker. How much quicker?  In a
// simple test with a single file, adding any of vector, string, map etc.
// added around 100ms of compilation time (each). I believe I've seen much worse,
// but just multiply 100-400ms by the number of files in a large project and
// there may be a good argument for avoiding to include STL headers based on
// build time.  (TODO: refer to example program).
//
// In fairness, this problem may be partially solved with precompiled headers,
// but those come with some issues too. (build setup, pollution, still have to
// compile on each rebuild or precompiled header change).
//
// With the Alloc_Slice class, methods like operator[] have been marked as
// "artificial", meaning it's easier to debug code without jumping all over the
// place. With std::vector and similar classes, I believe there is no way, or
// no standardized way, to build such that we don't jump around files like wild
// when debugging.
//
// If these arguments haven't been convincing, I'll end it now anyway -- the
// text is already much bigger than the actual code.

template<typename T>
class Alloc_Slice
{
   T *m_ptr = nullptr;
   size_t m_capacity = 0;

public:

   __pmq_artificial_method
   T& operator[](size_t i) const
   {
      return m_ptr[i];
   }

   __pmq_artificial_method
   T *data() const
   {
      return m_ptr;
   }

   __pmq_artificial_method
   size_t capacity() const
   {
      return m_capacity;
   }

   __pmq_artificial_method
   Slice<T> slice() const
   {
      return Slice<T>(m_ptr, m_capacity);
   }

   __pmq_artificial_method
   Untyped_Slice untyped_slice() const
   {
      return slice().untyped();
   }

   void allocate(size_t capacity)
   {
      assert(! m_ptr);
      m_ptr = new T[capacity];
      m_capacity = capacity;
   }

   ~Alloc_Slice()
   {
      delete[] m_ptr;
   }
};


// Posix_FD
//
// Simple file-descriptor holder. The only important purpose is automatically
// closing the fd in the destructor. Setting the fd can happen in the
// constructor or be delayed. A new fd can be set after closing the old one.
// The fd can be retrieved using the .get() method. There are no other methods
// defined, the point here is not to make an abstraction over FDs but just to
// auto-close it.
//
// There is not much more to say. A concern was brought up was that it would be
// better to use an existing class. Again, it's important to note that we're
// not trying to add some (probably ill-defined) abstraction. The fact that
// this class stores fds is not hidden and there isn't any I/O functionality
// contained.
//
// Given this, I wasn't sure what existing class to use that does the same
// thing. This Posix_FD class was quick and easy to write and I hope it is easy
// to read too.
//
// Another concern was that we shouldn't use close() directly here, but instead
// use an abstraction (from an existing library) that papers over platform
// differences such that the code can work on e.g. Windows too. (Windows has a
// Poxix FS layer as well but the code probably wouldn't work without extra
// work and handling of subtle differences).
//
// I can understand this concern, however BeeGFS can not be easily ported to
// e.g. Windows anyway, and this has never been a declared goal of the project.
// BeeGFS currently can't build on Windows and probably never will.
//
// The usage code currently makes non-trivial use of advanced POSIX and Linux
// functions, such as openat(), fsync(), mmap(), pread(), pwrite(). sendfile()
// was used earlier, and might come back. We rely on Posix file permissions
// too, and on certain semantics like for example O_CREAT | O_EXCL during file
// creation.
//
// I'm not aware of a better API that is more portable while providing the same
// functionality.
//
// Also, papering over platform differences may be harder than it initially
// sounds as soon as good performance and thus good control and good error
// handling is a requirement. To be portable, special handling of platform
// idiosyncracies might be required, and the architecture would have to change
// anyway: away from synchronous function calls which would make the
// abstraction leak into the core code, and towards a more asynchronous model
// that is better decoupled from the core code.
//
// It was proposed that std::ifstream / std::ofstream (or similar standardized
// class) could be used instead. std::ifstream in particular would be a bad fit
// since it is a very generic class that comes with buffering and formatting by
// default. I can't easily see how to replace the calls I listed above using
// std::ifstream. Event if it's possible, the result may be more complicated /
// require use of the underlying Posix FD anyway / be less clear / be more code
// / require to give up some control over syscalls etc. ifstream uses
// exceptions and has facilities such as formatting that aren't needed, but the
// presence of this attached functionality would make the purpose less clear
// IMO.
//

class Posix_FD
{
   int m_fd = -1;

public:

   __pmq_artificial_method
   int get()
   {
      return m_fd;
   }

   __pmq_artificial_method
   bool valid()
   {
      return m_fd != -1;
   }

   int close_fd()
   {
      int ret = 0;
      if (m_fd != -1)
      {
         ret = close(m_fd);
         m_fd = -1;
      }
      return ret;
   }

   __pmq_artificial_method
   void set(int fd)
   {
      assert(m_fd == -1);
      m_fd = fd;
   }

   __pmq_artificial_method
   void operator=(int fd)
   {
      set(fd);
   }

   __pmq_artificial_method
   Posix_FD()
   {
   }

   __pmq_artificial_method
   Posix_FD(int fd)
   {
      set(fd);
   }

   __pmq_artificial_method
   ~Posix_FD()
   {
      close_fd();
   }
};


//
// Libc_DIR
//
// Similar to Posix_FD, but for libc DIR * handles. Same rationale for why I've
// written this applies as for Posix_FD.
//
// This class is currently not used so could be removed.
//

class Libc_DIR
{
   DIR *m_dir = nullptr;

   public:

   __pmq_artificial_method
   bool valid()
   {
      return m_dir != nullptr;
   }

   __pmq_artificial_method
   DIR *get()
   {
      return m_dir;
   }

   __pmq_artificial_method
   void set(DIR *dir)
   {
      assert(m_dir == nullptr);
      m_dir = dir;
   }

   void close_dir()
   {
      if (m_dir)
      {
         closedir(m_dir);
         m_dir = nullptr;
      }
   }

   __pmq_artificial_method
   void operator=(DIR *dir)
   {
      set(dir);
   }

   __pmq_artificial_method
   Libc_DIR()
   {
   }

   __pmq_artificial_method
   Libc_DIR(DIR *dir)
   {
      m_dir = dir;
   }

   __pmq_artificial_method
   ~Libc_DIR()
   {
      close_dir();
   }
};

//
// Mmap_Region
//
// Similar to Posix_FD, but for memory mappings.
//
// On destruction, unmaps the mapped region using munmap().
//

class MMap_Region
{
   void *m_ptr = MAP_FAILED;
   size_t m_length = 0;

public:

   __pmq_artificial_method
   void *get() const
   {
      return m_ptr;
   }

   __pmq_artificial_method
   Untyped_Slice untyped_slice() const
   {
      return Untyped_Slice(m_ptr, m_length);
   }

   __pmq_artificial_method
   bool valid()
   {
      return m_ptr != MAP_FAILED;
   }

   void close_mapping()
   {
      if (m_ptr != MAP_FAILED)
      {
         if (munmap(m_ptr, m_length) == -1)
         {
            // should not happen. Simply printing the error for now
            pmq_perr_ef(errno, "WARNING: munmap() failed");
         }
         m_ptr = MAP_FAILED;
         m_length = 0;
      }
   }

   // like mmap but returns whether successful
   bool create(void *addr, size_t newlength, int prot, int flags,
         int fd, off_t offset)
   {
      assert(m_ptr == MAP_FAILED);
      void *newptr = mmap(addr, newlength, prot, flags, fd, offset);
      if (newptr == MAP_FAILED)
         return false;
      m_ptr = newptr;
      m_length = newlength;
      return true;
   }

   __pmq_artificial_method
   ~MMap_Region()
   {
      close_mapping();
   }
};


// Mutex_Protected
//
// Simple wrapper class that protects a data item with a mutex.
// The load() and store() mutex implement thread-synchronized read and write
// access to the data item by locking the resource with a mutex during the
// operation.
//
// A class like Folly::Synchronized might replace this. But again, this was
// very easy to write and is extremely small. Pulling in a large dependency
// just for that might not be justified. Also, having our own class allows
// choosing the mutex type. For example, if we want to profile mutexes using
// the Tracy frame profiler, we need to use Tracy's mutex wrappers (here,
// hidden in the PMQ_PROFILED_MUTEX wrapper). While Folly::Synchronized supports
// custom mutexes, one would need to understand and impleemnt "the extended
// protocol implemented in folly/synchronized/Lock.h".
//
// Upon quick browsing of the 1000 lines in Lock.h, it isn't immediately clear
// what that protocol entails and how much work it would be (if any) to wrap
// our own mutex type (which is potentially a wrap of std::mutex already) to
// conform to that protocol.
//
// Maybe there is something in the C++ standard that is suited as a
// replacement?
//
// Maybe there is, but I consider it much easier to just write 2 methods
// totalling 4 straightforward lines of code...
//

template<typename T>
class Mutex_Protected
{
   PMQ_PROFILED_MUTEX(m_mutex);
   T m_value;

public:

   void store(T value)
   {
      PMQ_PROFILED_LOCK(lock_, m_mutex);
      m_value = value;
   }

   T load()
   {
      PMQ_PROFILED_LOCK(lock_, m_mutex);
      return m_value;
   }
};


/*
 * String "slice" that can be passed around. No lifetime semantics or
 * unexpected copying etc.
 *
 * We could use std::string_view instead, but that is a templated type. The
 * idea of PMQ_String is to wrap just a char-pointer with a size, and nothing
 * more, to have a package that one can ship around. We mostly use strings for
 * printf-style formatting and to open files, and we don't need or want any
 * more complicated semantics than that.
 */
struct PMQ_String
{
   const char *buffer;
   size_t size;
};

/*
 * Simple string "holder" class that allocates and frees its buffer. The
 * contained string is immutable once constructed. But a new one can be
 * "swapped" in by dropping the old string and creating a new one.
 *
 * Is this a case of NIH when there is std::string? Maybe, but basically the
 * same arguments as for Alloc_Slice and the other classes above apply.
 *
 * std::string
 *
 *  - is somewhat slow to compile
 *  - Unexpected allocations / copies (and thus exceptions as well) can happen
 *    very easily, without anyone noticing -- For example, it's as easy as
 *    writing "auto x = y" instead of "auto& x = y".
 *  - Apart from exceptions and copies / resizes, appending, there is more
 *    complexity that we don't need and don't want and that would actually be a
 *    misfit for our project. Ugly error messages with huge types (...
 *    std::basic_char ... etc.) is only a small symptom of this.
 */
class PMQ_Owned_String
{
   PMQ_String m_string = {};

public:

   bool valid() const
   {
      return m_string.buffer != nullptr;
   }

   __pmq_artificial_method
   PMQ_String get() const
   {
      return m_string;
   }

   void drop()
   {
      // Checking only for clarity. free() and the rest of the code would work
      // with a null buffer too.
      if (m_string.buffer != nullptr)
      {
         free((void *) m_string.buffer);
         m_string.buffer = nullptr;
         m_string.size = 0;
      }
   }

   void set(const char *buffer)
   {
      assert(! m_string.buffer);
      char *copy = strdup(buffer);
      if (copy == nullptr)
      {
         // is an exception what we want / need?
         throw std::bad_alloc();
      }
      m_string.buffer = copy;
      m_string.size = strlen(buffer);
   }

   __pmq_artificial_method
   PMQ_Owned_String()
   {
      m_string.buffer = nullptr;
      m_string.size = 0;
   }

   ~PMQ_Owned_String()
   {
      drop();
   }
};



/*
 * SNs (sequence numbers)
 *
 * Sequence numbers, and the ringbuffers that build on them, are a core concept
 * of how the PMQ works.
 *
 * I believe they are pretty much what is elsewhere known as "LMAX Disruptor"
 * (google it).
 *
 * Sequence numbers are 64-bit unsigned integers that can wraparound (but this
 * is only theoretical -- wraparound is probably completely untested since
 * 64-bit numbers don't overflow easily in practice).
 *
 * Ringbuffers have a number of slots that is 2^N for some N. SN's are mapped
 * to slots with wrap-around in the ringbuffer's 2^N slots by using the lowest
 * N bits of the SN to index into the slots array.
 *
 * The SN templated class provides some type safety -- the Tag type is a
 * "phantom tag" (can be implemented by making a new "empty" class) that
 * prevents indexing into a ringbuffer using a mismatching sequence number. For
 * example, we have a ringbuffer of input-slots that should be indexed by *slot
 * sequence numbers* (SSNs). And we have a ringbuffer of chunks that should be
 * indexed by *chunk sequence numbers (CSNs). The on-disk chunk store is
 * another kind of ringbuffer that works with the same principle of wrapping
 * around automatically.
 *
 * We also track *message sequence numbers* (MSNs) but we don't use them for
 * indexing, only for binary search.
 *
 * Mathematically, SNs form an affine space. This is like a vector space but
 * without a designated origin (pls forgive me if what I write here is slightly
 * incorrect as far as mathematics is concerned. Only the idea matters). There
 * is a 0 value, but it is not meaningfully different compared to any other
 * value.
 *
 * One can subtract two sequence numbers to get a distance (represented as bare
 * uint64_t), and one can add a distance to a sequence number to get a new
 * sequence number. However, unlike a vector space with designated 0, one can
 * not add two sequence numbers meaningfully (SN<T> has operator+(uint64_t d)
 * but no operator+(SN<T>& other).
 */

template<typename Tag>
class SN
{
   uint64_t m_value;

public:

   explicit SN(uint64_t value)
   {
      m_value = value;
   }

   // Some C++ trivia following. In most cases you can ignore this and just use
   // the class similar to primitive integers.
   //
   // Here we specify an *explicitly-defaulted default-constructor*. This will
   // allow us to initialize the object with undefined (garbage) value if we
   // want so.
   //
   // Explanation: Since we have explicitly specified the constructor with 1
   // argument already, there wouldn't be an implicit default constructor (a
   // constructor with no arguments). To get a default constructor, we need to
   // explicitly specify one.  We need a default constructor (no constructor
   // arguments) if we want to write
   //
   // SN sn;
   //
   // For simple data types (like SN), we typically want the above line to
   // leave the object's members uninitialized (garbage values). While this is
   // in some ways dangerous, it can be simpler especially for objects where
   // zero-initialization isn't very convenient or meaningful. Leaving values
   // uninitialized in the default constructor also allows the compiler to
   // catch bugs in some situations when the user unintentionally forgot to
   // specify an explicit value.
   //
   // Note a gotcha: There is a difference between an empty default constructor
   //
   // SN() {}
   //
   // and an (explicitly or implicitly) defaulted default constructor:
   //
   // SN() = default;
   //
   // If we use the class like this:
   //
   //   SN x {};
   //   SN y = SN();  // or like this
   //   SN z = {};  // or like this...
   //
   // then x will contain garbarge with the empty default constructor, but will
   // be zero-initialized with the (explicitly-) defaulted default constructor.
   // We'd typically want zero initialization with this syntax.

   SN() = default;

   __pmq_artificial_method
   uint64_t value() const
   {
      return m_value;
   }

   __pmq_artificial_method
   void operator++()
   {
      m_value++;
   }

   __pmq_artificial_method
   void operator++(int)
   {
      m_value++;
   }

   __pmq_artificial_method
   SN operator+=(uint64_t d)
   {
      m_value += d;
      return *this;
   }

   __pmq_artificial_method
   SN& operator-=(uint64_t d)
   {
      m_value -= d;
      return *this;
   }

   __pmq_artificial_method
   SN operator+(uint64_t d) const
   {
      return SN(m_value + d);
   }

   __pmq_artificial_method
   SN operator-(uint64_t d) const
   {
      return SN(m_value - d);
   }

   __pmq_artificial_method
   uint64_t operator-(SN other) const
   {
      return m_value - other.m_value;
   }

   __pmq_artificial_method
   bool operator==(SN other) const
   {
      return m_value == other.m_value;
   }

   __pmq_artificial_method
   bool operator!=(SN other) const
   {
      return m_value != other.m_value;
   }
};

/*
 * COMPARING SEQUENCE NUMBERS
 * ==========================
 *
 * Since sequence numbers wrap around (in theory, when 64 bits overflow) they
 * have no natural ordering.
 *
 * However, in practice, sequence numbers are used to index in much smaller
 * buffer, and at any given time there is only a small window of sequence
 * numbers. It's a sliding window, but a window still.
 *
 * So, admitting that the sequence numbers in a given window may wraparound,
 * back to 0, we can still assume that they never "overtake" each other.
 * We can subtract two numbers using unsigned arithmetic and determine their
 * relative ordering from the result. Centering our worldview at a number x, we
 * divide the space of uint64_t numbers into those that are less than x (x -
 * 2^63 to x) and those that are greater than x (x to 2^63).
 *
 * Note that this relation is not transitive (x <= y && y <= z does not imply x
 * <= z), and not antisymmetric -- (x + 2^63) is both greater and less than x.
 * So it's not a true ordering relation, but in practice we can use it to
 * reliably compare items by "age".
 *
 * The value 1 should be considered greater than UINT64_MAX, since 1 -
 * UINT64_MAX == 2.  Conversely, UINT64_MAX is less than 1 since UINT64_MAX - 1
 * equals (UINT64_MAX - 1), which is a.
 *
 */


// Comparing bare uint64_t sequence values.

__pmq_artificial_func
bool _sn64_lt(uint64_t a, uint64_t b)
{
   return b - (a + 1) <= UINT64_MAX / 2;
}

__pmq_artificial_func
bool _sn64_le(uint64_t a, uint64_t b)
{
   return b - a <= UINT64_MAX / 2;
}

__pmq_artificial_func
bool _sn64_ge(uint64_t a, uint64_t b)
{
   return a - b <= UINT64_MAX / 2;
}

__pmq_artificial_func
bool _sn64_gt(uint64_t a, uint64_t b)
{
   return a - (b + 1) <= UINT64_MAX / 2;
}



// Comparing type-safe "tagged" SN values

template<typename Tag>
__pmq_artificial_func
bool sn64_lt(SN<Tag> a, SN<Tag> b)
{
   return b - (a + 1) <= UINT64_MAX / 2;
}

template<typename Tag>
__pmq_artificial_func
bool sn64_le(SN<Tag> a, SN<Tag> b)
{
   return b - a <= UINT64_MAX / 2;
}

template<typename Tag>
__pmq_artificial_func
bool sn64_ge(SN<Tag> a, SN<Tag> b)
{
   return a - b <= UINT64_MAX / 2;
}

template<typename Tag>
__pmq_artificial_func
bool sn64_gt(SN<Tag> a, SN<Tag> b)
{
   return a - (b + 1) <= UINT64_MAX / 2;
}

template<typename Tag>
__pmq_artificial_func
bool sn64_inrange(SN<Tag> sn, SN<Tag> lo, SN<Tag> hi)
{
   return sn - lo <= hi - lo;
}



// Ringbuffer containing a buffer (power-of-2 size) of element of type V. It
// can be "indexed" using SN's of matching type.

template<typename Tag, typename V>
class Ringbuffer
{
   using K = SN<Tag>;

   V *m_ptr = nullptr;
   size_t m_count = 0;

public:

   __pmq_artificial_method
   uint64_t slot_count() const
   {
      return m_count;
   }

   __pmq_artificial_method
   void reset(Slice<V> slice)
   {
      assert(pmq_is_power_of_2(slice.count()));
      m_ptr = slice.data();
      m_count = slice.count();
   }

   __pmq_artificial_method
   Slice<V> as_slice() const
   {
      return Slice<V>(m_ptr, m_count);
   }

   __pmq_artificial_method
   const V *get_slot_for(K k) const
   {
      return &m_ptr[k.value() & (m_count - 1)];
   }

   __pmq_artificial_method
   V *get_slot_for(K k)
   {
      return &m_ptr[k.value() & (m_count - 1)];
   }

   __pmq_artificial_method
   Ringbuffer()
   {
   }

   __pmq_artificial_method
   Ringbuffer(V *ptr, uint64_t size)
   {
      reset(ptr, size);
   }
};
