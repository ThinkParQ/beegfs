#ifndef OPEN_OSDEPS_H_
#define OPEN_OSDEPS_H_

#include <filesystem/FhgfsOps_versions.h>

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>


#ifdef BEEGFS_DEBUG
   extern void* os_saveStackTrace(void);
   extern void os_printStackTrace(void * trace, int spaces);
   extern void os_freeStackTrace(void *trace);
#endif // BEEGFS_DEBUG


// inliners
static inline void* os_kmalloc(size_t size);
static inline void* os_kzalloc(size_t size);

static inline int os_strnicmp(const char* s1, const char* s2, size_t n);


void* os_kmalloc(size_t size)
{
   void* buf = kmalloc(size, GFP_NOFS);

   if(unlikely(!buf) )
   {
      printk(KERN_WARNING BEEGFS_MODULE_NAME_STR ": kmalloc of '%d' bytes failed. Retrying...\n", (int)size);
      buf = kmalloc(size, GFP_NOFS | __GFP_NOFAIL);
      printk(KERN_WARNING BEEGFS_MODULE_NAME_STR ": kmalloc retry of '%d' bytes succeeded\n", (int)size);
   }

   return buf;
}

void* os_kzalloc(size_t size)
{
   void* buf = kzalloc(size, GFP_NOFS);

   if(unlikely(!buf) )
   {
      printk(KERN_WARNING BEEGFS_MODULE_NAME_STR ": kzalloc of '%d' bytes failed. Retrying...\n", (int)size);
      buf = kzalloc(size, GFP_NOFS | __GFP_NOFAIL);
      printk(KERN_WARNING BEEGFS_MODULE_NAME_STR ": kzalloc retry of '%d' bytes succeeded\n", (int)size);
   }

   return buf;
}

/**
 * strncasecmp was broken in the linux kernel pre-3.18. strnicmp was
 * implemented correctly in that timeframe. In kernel >= 3.18, strnicmp
 * is either a wrapper for strncasecmp or is not defined.
 */
int os_strnicmp(const char *s1, const char *s2, size_t n)
{
   #ifdef KERNEL_HAS_STRNICMP
      return strnicmp(s1, s2, n);
   #else
      return strncasecmp(s1, s2, n);
   #endif
}

#endif /* OPEN_OSDEPS_H_ */
