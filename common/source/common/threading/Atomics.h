#ifndef ATOMICS_H_
#define ATOMICS_H_

#include <common/Common.h>


template <class TemplateType> class Atomic;

typedef Atomic<size_t>   AtomicSizeT;
typedef Atomic<ssize_t>  AtomicSSizeT;
typedef Atomic<uint32_t> AtomicUInt32;
typedef Atomic<uint64_t> AtomicUInt64;
typedef Atomic<int16_t>  AtomicInt16;
typedef Atomic<int64_t>  AtomicInt64;

#if !defined(__has_feature)
# define BEEGFS_NO_TSAN
#else
# if !__has_feature(thread_sanitizer)
#  define BEEGFS_NO_TSAN
# else
#  define BEEGFS_NO_TSAN __attribute__((no_sanitize_thread))
# endif
#endif


/*
 * Atomic operations - wrapper for gcc extensions, internally it makes use of memory barriers.
 * Note: We use gcc architechture preprocessor defines here and also gcc atomic extensions.
 *
 * @TemplateType see gcc builtin atomics documentation for types (int, int64_t, ...)
 */
template <class TemplateType>
class Atomic
{
   public:
      /**
       * Initialize and set to 0.
       */
      Atomic()
      {
         this->setZero(); // initialize with zero
      }

      /**
       * Initialize with given value.
       */
      Atomic(TemplateType value)
      {
         set(value);
      }

   private:
      TemplateType atomicValue; // atomic value we are operating on

   public:

      /**
       * Set to given value.
       */
      BEEGFS_NO_TSAN
      void set(TemplateType value)
      {
      #if (defined __x86_64__ || defined __i386__ || defined __i686__)
         // special assumptions for x86 and x86_64 (actually beginning with i486)
         this->atomicValue = value;
      #else
         /* note: the strange name of __sync_lock_test_and_set() is because this method was
            originally intended for spinlocks, in which case the value was set to 1 (=> lock) and
            the counter-part is __sync_lock_release(), which resets the value to 0 (=> unlock). */

         __sync_lock_test_and_set(&this->atomicValue, value); // gcc extension
      #endif
      }


      /**
       * Set to 0.
       */
      BEEGFS_NO_TSAN
      void setZero()
      {
      #if (defined __x86_64__ || defined __i386__ || defined __i686__)
         // special assumptions for x86 and x86_64 (actually beginning with i486)
         this->atomicValue = 0;
      #else
         // note: if you're wondering about the name __sync_lock_release(), read the set() comment

         __sync_lock_release(&this->atomicValue); // gcc extension
      #endif
      }

      /**
       * Increase by one.
       *
       * @return previous value
       */
      BEEGFS_NO_TSAN
      TemplateType increase()
      {
         return __sync_fetch_and_add(&this->atomicValue, 1); // NOTE: gcc extension
      }

      /**
       * Increase by value.
       *
       * @return previous value
       */
      BEEGFS_NO_TSAN
      TemplateType increase(TemplateType value)
      {
         return __sync_fetch_and_add(&this->atomicValue, value); // NOTE: gcc extension
      }

      /**
       * Decrease by one.
       *
       * @return previous value
       */
      BEEGFS_NO_TSAN
      TemplateType decrease()
      {
         return __sync_fetch_and_sub(&this->atomicValue, 1); // NOTE: gcc extension
      }

      /**
       * Decrease by value.
       *
       * @return previous value
       */
      BEEGFS_NO_TSAN
      TemplateType decrease(TemplateType value)
      {
         return __sync_fetch_and_sub(&this->atomicValue, value); // NOTE: gcc extension
      }

      /**
       * Set to given value if the
       */
      BEEGFS_NO_TSAN
      bool compareAndSet(TemplateType value, TemplateType compareValue)
      {
         return __sync_bool_compare_and_swap(&this->atomicValue, compareValue, value);
      }

      /**
       * Get the atomic value.
       */
      BEEGFS_NO_TSAN
      TemplateType read() const
      {
      #if (defined __x86_64__ || defined __i386__ || defined __i686__)
         // special assumptions for x86 and x86_64 (actually beginning with i486)
         return (*(volatile TemplateType* )&(this)->atomicValue);
      #else
         /* There is no special read function, but we can simply add zero to get the same effect.
          * Also just reading the variable was tested to work, but using __sync_fetch_and_add()
          * we make absolutely sure we read the atomic value.
          *
          * (would be nice if gcc could optimize out this command itself, so that we wouldn't
          * need the entire preprocessor construct, but disassembling a test program showed
          * gcc adds a "lock xadd" command, which is slow)
          */
         return __sync_fetch_and_add((TemplateType*) &this->atomicValue, 0);
      #endif
      }


};

#undef BEEGFS_NO_TSAN


#endif /* ATOMICS_H_ */
