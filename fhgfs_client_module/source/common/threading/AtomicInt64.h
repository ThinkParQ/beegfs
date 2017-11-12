#ifndef OPEN_AtomicInt64_H_
#define OPEN_AtomicInt64_H_

#include <common/Common.h>

#include <asm/atomic.h> // also adds ATOMIC64_INIT if available


#ifndef ATOMIC64_INIT
   #include <os/atomic64.h>
#endif


struct AtomicInt64;
typedef struct AtomicInt64 AtomicInt64;


static inline void AtomicInt64_init(AtomicInt64* this, uint64_t initialVal);
static inline void AtomicInt64_uninit(AtomicInt64* this);

static inline uint64_t AtomicInt64_incAndRead(AtomicInt64* this);
static inline void AtomicInt64_inc(AtomicInt64* this);
static inline void AtomicInt64_dec(AtomicInt64* this);
static inline uint64_t AtomicInt64_read(AtomicInt64* this);
static inline void AtomicInt64_set(AtomicInt64* this, uint64_t newValue);

#if 0 // not available in old kernels

static inline uint64_t AtomicInt64_compareAndSwap(
   AtomicInt64* this, uint64_t oldValue, uint64_t newValue);
static inline void AtomicInt64_max(AtomicInt64* this, uint64_t minValue);

#endif // if 0 // not available in old kernels


struct AtomicInt64
{
   atomic64_t kernelAtomic64;
};


void AtomicInt64_init(AtomicInt64* this, uint64_t initialVal)
{
#ifdef ATOMIC64_INIT
   AtomicInt64 initializer =
   {
      .kernelAtomic64 = ATOMIC64_INIT(initialVal),
   };

   *this = initializer;
#else
   atomic_init(&this->kernelAtomic64, initialVal);
#endif
}

void AtomicInt64_uninit(AtomicInt64* this)
{
   // nothing to be done here
}

/**
 * Increase and return the new value
 */
uint64_t AtomicInt64_incAndRead(AtomicInt64* this)
{
   return atomic64_inc_return(&this->kernelAtomic64);
}

/**
 * Increase by 1.
 */
void AtomicInt64_inc(AtomicInt64* this)
{
   atomic64_inc(&this->kernelAtomic64);
}

/**
 * Decrease by 1.
 */
void AtomicInt64_dec(AtomicInt64* this)
{
   atomic64_dec(&this->kernelAtomic64);
}

uint64_t AtomicInt64_read(AtomicInt64* this)
{
   return atomic64_read(&this->kernelAtomic64);
}

/**
 * Set to "newValue".
 */
void AtomicInt64_set(AtomicInt64* this, uint64_t newValue)
{
   atomic64_set(&this->kernelAtomic64, newValue);
}

#if 0 // not available in old kernel version
/**
 * Test whether current value matches "oldValue" and only if it matches, update it to "newValue".
 *
 * @return   the value *before* the swap, regardless if the swap was successful or not
 */
uint64_t AtomicInt64_compareAndSwap(AtomicInt64* this, uint64_t oldValue, uint64_t newValue)
{
   return atomic64_cmpxchg(&this->kernelAtomic64, oldValue, newValue);
}


/**
 * Increase current value to "minValue" if it is currently smaller, otherwise leave it as it was.
 *
 * Note: Use this carefully and only in special scenarios, because it could run endless if the
 * atomic value is not monotonically increasing.
 */
void AtomicInt64_max(AtomicInt64* this, uint64_t minValue)
{
   uint64_t currentVal = AtomicInt64_read(this);
   for( ; ; )
   {
      uint64_t swapRes;

      if(currentVal >= minValue)
         return; // no need to update it at all

      swapRes = AtomicInt64_compareAndSwap(this, currentVal, minValue);
      if (swapRes == currentVal)
         return;

      currentVal = swapRes; // swap was not successful, update currentVal
   }
}

#endif // if 0 // not available in old kernel version


#endif /* OPEN_AtomicInt64_H_ */
