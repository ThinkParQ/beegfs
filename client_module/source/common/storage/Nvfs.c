/*
 * Copyright (c)2018, NVIDIA CORPORATION. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifdef BEEGFS_NVFS
#include <app/log/Logger.h>
#include "Nvfs.h"

struct nvfs_dma_rw_ops *nvfs_ops = NULL;

atomic_t nvfs_shutdown = ATOMIC_INIT(1);

DEFINE_PER_CPU(long, nvfs_n_ops);

int REGISTER_FUNC (struct nvfs_dma_rw_ops *ops)
{
   printk_fhgfs_debug(KERN_INFO, "%s:%d: register nvfs ops\n",
                      __func__, __LINE__);
   if (NVIDIA_FS_CHECK_FT_SGLIST_DMA(ops) &&
      NVIDIA_FS_CHECK_FT_GPU_PAGE(ops) &&
      NVIDIA_FS_CHECK_FT_DEVICE_PRIORITY(ops))
   {
      nvfs_ops = ops;
      atomic_set(&nvfs_shutdown, 0);
      return 0;
   }
   return -ENOTSUPP;
}
EXPORT_SYMBOL(REGISTER_FUNC);

void UNREGISTER_FUNC (void)
{
   int ops;
   printk_fhgfs_debug(KERN_INFO, "%s:%d: begin unregister nvfs\n",
                      __func__, __LINE__);
   (void) atomic_cmpxchg(&nvfs_shutdown, 0, 1);
   do
   {
      msleep(NVFS_HOLD_TIME_MS);
      ops = nvfs_count_ops();
      printk_fhgfs_debug(KERN_INFO, "%s:%d: nvfs_count_ops=%d\n",
         __func__, __LINE__, ops);
   }
   while (ops);
   nvfs_ops = NULL;
   printk_fhgfs_debug(KERN_INFO, "%s:%d: unregister nvfs complete\n",
                      __func__, __LINE__);

}
EXPORT_SYMBOL(UNREGISTER_FUNC);

#endif /* BEEGFS_NVFS */
