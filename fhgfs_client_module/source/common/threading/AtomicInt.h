#ifndef OPEN_ATOMICINT_H_
#define OPEN_ATOMICINT_H_

#include <common/Common.h>

#include <asm/atomic.h>


struct AtomicInt;
typedef struct AtomicInt AtomicInt;


static inline void AtomicInt_init(AtomicInt* this, int initialVal);
static inline void AtomicInt_uninit(AtomicInt* this);

static inline int AtomicInt_incAndRead(AtomicInt* this);
static inline void AtomicInt_inc(AtomicInt* this);
static inline void AtomicInt_dec(AtomicInt* this);
static inline int AtomicInt_read(AtomicInt* this);
static inline void AtomicInt_set(AtomicInt* this, int newValue);
static inline int AtomicInt_compareAndSwap(AtomicInt* this, int oldValue, int newValue);
static inline void AtomicInt_max(AtomicInt* this, int minValue);
static inline int AtomicInt_decAndTest(AtomicInt* this);


struct AtomicInt
{
   atomic_t kernelAtomic;
};


void AtomicInt_init(AtomicInt* this, int initialVal)
{
   AtomicInt initializer =
   {
      .kernelAtomic = ATOMIC_INIT(initialVal),
   };

   *this = initializer;
}

void AtomicInt_uninit(AtomicInt* this)
{
   // nothing to be done here
}

/**
 * Increase and return the new value
 */
int AtomicInt_incAndRead(AtomicInt* this)
{
   return atomic_inc_return(&this->kernelAtomic);
}

/**
 * Increase by 1.
 */
void AtomicInt_inc(AtomicInt* this)
{
   atomic_inc(&this->kernelAtomic);
}

/**
 * Decrease by 1.
 */
void AtomicInt_dec(AtomicInt* this)
{
   atomic_dec(&this->kernelAtomic);
}

int AtomicInt_read(AtomicInt* this)
{
   return atomic_read(&this->kernelAtomic);
}

/**
 * Set to "newValue".
 */
void AtomicInt_set(AtomicInt* this, int newValue)
{
   atomic_set(&this->kernelAtomic, newValue);
}

/**
 * Test whether current value matches "oldValue" and only if it matches, update it to "newValue".
 *
 * @return   the value *before* the swap, regardless if the swap was successful or not
 */
int AtomicInt_compareAndSwap(AtomicInt* this, int oldValue, int newValue)
{
   return atomic_cmpxchg(&this->kernelAtomic, oldValue, newValue);
}

/**
 * Increase current value to "minValue" if it is currently smaller, otherwise leave it as it was.
 *
 * Note: Use this carefully and only in special scenarios, because it could run endless if the
 * atomic value is not monotonically increasing.
 */
void AtomicInt_max(AtomicInt* this, int minValue)
{
   int currentVal = AtomicInt_read(this);
   for( ; ; )
   {
      int swapRes;

      if(currentVal >= minValue)
         return; // no need to update it at all

      swapRes = AtomicInt_compareAndSwap(this, currentVal, minValue);
      if (swapRes == currentVal)
         return;

      currentVal = swapRes; // swap was not successful, update currentVal
   }
}

/**
 * Decrease by one and test if the counter is 0
 *
 * @return 1 if the counter is zero, 0 in all other cases
 */
int AtomicInt_decAndTest(AtomicInt* this)
{
   return atomic_dec_and_test(&this->kernelAtomic);
}


#endif /* OPEN_ATOMICINT_H_ */
