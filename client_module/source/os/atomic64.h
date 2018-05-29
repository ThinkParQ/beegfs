#include <common/Common.h>

#include <asm/atomic.h> // also adds ATOMIC64_INIT if available


#ifndef ATOMIC64_INIT // basic test if the kernel already provides atomic64_t

/*
 * Note: Below is the atomic64.c copied from linux-git
 */


/*
 * Generic implementation of 64-bit atomics using spinlocks,
 * useful on processors that don't have 64-bit atomic instructions.
 *
 * Copyright © 2009 Paul Mackerras, IBM Corp. <paulus@au1.ibm.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
#ifndef _ASM_GENERIC_ATOMIC64_H
#define _ASM_GENERIC_ATOMIC64_H

typedef struct {
	long long counter;
	spinlock_t lock;  // added for fhgfs
} atomic64_t;

// #define ATOMIC64_INIT(i)	{ (i) } // disabled for fhgfs

static inline void atomic_init(atomic64_t *atomic, uint64_t value); // added for fhgfs

extern long long atomic64_read(const atomic64_t *v);
extern void	 atomic64_set(atomic64_t *v, long long i);
extern void	 atomic64_add(long long a, atomic64_t *v);
extern long long atomic64_add_return(long long a, atomic64_t *v);
extern void	 atomic64_sub(long long a, atomic64_t *v);
extern long long atomic64_sub_return(long long a, atomic64_t *v);
extern long long atomic64_dec_if_positive(atomic64_t *v);
extern long long atomic64_cmpxchg(atomic64_t *v, long long o, long long n);
extern long long atomic64_xchg(atomic64_t *v, long long new);
extern int	 atomic64_add_unless(atomic64_t *v, long long a, long long u);

#define atomic64_add_negative(a, v)	(atomic64_add_return((a), (v)) < 0)
#define atomic64_inc(v)			atomic64_add(1LL, (v))
#define atomic64_inc_return(v)		atomic64_add_return(1LL, (v))
#define atomic64_inc_and_test(v) 	(atomic64_inc_return(v) == 0)
#define atomic64_sub_and_test(a, v)	(atomic64_sub_return((a), (v)) == 0)
#define atomic64_dec(v)			atomic64_sub(1LL, (v))
#define atomic64_dec_return(v)		atomic64_sub_return(1LL, (v))
#define atomic64_dec_and_test(v)	(atomic64_dec_return((v)) == 0)
#define atomic64_inc_not_zero(v) 	atomic64_add_unless((v), 1LL, 0LL)

/*
 * Initializer for fhgfs, replacement for  ATOMIC64_INIT(i)
 */
void atomic_init(atomic64_t* atomic, uint64_t value)
{
   spin_lock_init(&atomic->lock);
   atomic->counter = value;
}


#endif  /*  _ASM_GENERIC_ATOMIC64_H  */

#endif // #ifndef ATOMIC64_INIT
