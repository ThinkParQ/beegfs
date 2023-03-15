#ifndef BEEGFS_NVFS_H
#define BEEGFS_NVFS_H

/**
 *  BeeGFS and NVIDIA have name collisions on MIN and MAX. Briefly undefine MIN and MAX
 *  to allow NVIDIA headers to be included without warnings.
 */
#ifdef MIN
#pragma push_macro("MIN")
#undef MIN
#define BEEGFS_POP_MIN
#endif
#ifdef MAX
#pragma push_macro("MAX")
#undef MAX
#define BEEGFS_POP_MAX
#endif

#ifdef BEEGFS_NVFS
#define MODULE_PREFIX beegfs_v1

#include <linux/delay.h>
#include <nvfs-dma.h>

#define REGSTR2(x) x##_register_nvfs_dma_ops
#define REGSTR(x)  REGSTR2(x)

#define UNREGSTR2(x) x##_unregister_nvfs_dma_ops
#define UNREGSTR(x)  UNREGSTR2(x)

#define REGISTER_FUNC REGSTR(MODULE_PREFIX)
#define UNREGISTER_FUNC UNREGSTR(MODULE_PREFIX)

#define NVFS_IO_ERR                    -1
#define NVFS_CPU_REQ                   -2

#define NVFS_HOLD_TIME_MS 1000

extern struct nvfs_dma_rw_ops *nvfs_ops;

extern atomic_t nvfs_shutdown;

DECLARE_PER_CPU(long, nvfs_n_ops);

static inline long nvfs_count_ops(void)
{
   int i;
   long sum = 0;
   for_each_possible_cpu(i)
      sum += per_cpu(nvfs_n_ops, i);
   return sum;
}

static inline bool nvfs_get_ops(void)
{
   if (nvfs_ops && !atomic_read(&nvfs_shutdown)) {
      this_cpu_inc(nvfs_n_ops);
      return true;
   }
   return false;
}

static inline void nvfs_put_ops(void)
{
   this_cpu_dec(nvfs_n_ops);
}

/**
 *  Restore BeeGFS definitions of MIN and MAX.
 */
#ifdef BEEGFS_POP_MIN
#pragma pop_macro("MIN")
#undef BEEGFS_POP_MIN
#endif
#ifdef BEEGFS_POP_MAX
#pragma pop_macro("MAX")
#undef BEEGFS_POP_MAX
#endif

#endif /* BEEGFS_NVFS */
#endif /* BEEGFS_NVFS_H */
