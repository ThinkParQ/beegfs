#include <common/Common.h>
#include <os/OsDeps.h>
#include <common/FhgfsTypes.h>
#include <common/net/sock/NicAddress.h>
#include <common/net/sock/Socket.h>
#include <filesystem/FhgfsOps_versions.h>

#include <linux/netdevice.h>
#include <linux/in.h>
#include <linux/inetdevice.h>


#ifdef CONFIG_STACKTRACE
   #include <linux/stacktrace.h>
#endif

#define MAX_STACK_TRACE_CHAIN 16 // number of functions to to save in a stack trace


#ifdef BEEGFS_DEBUG

// Significant parts of the kernel code around struct stack_trace are removed
// when CONFIG_ARCH_STACKWALK is set. Code below needs to be rewritten to work
// with newer kernels that have CONFIG_ARCH_STACKWALK enabled.
#if defined CONFIG_STACKTRACE && !defined CONFIG_ARCH_STACKWALK

/**
 * Save a given trace. NOTE: Allocated memory has to be freed later on!
 */
void* os_saveStackTrace(void)
{
   struct stack_trace* trace;
   unsigned long *entries;

   trace = kmalloc(sizeof(struct stack_trace), GFP_NOFS);
   if (!trace)
      return NULL; // out of memory?

   entries = kmalloc(MAX_STACK_TRACE_CHAIN * sizeof(*entries), GFP_NOFS);
   if (!entries)
   { // out of memory?
      kfree(trace);
      return NULL;
   }

   trace->nr_entries = 0;
   trace->max_entries = MAX_STACK_TRACE_CHAIN;
   trace->entries = entries;
   trace->skip = 1; // cut off ourself, so 1

   save_stack_trace(trace);

   return trace;
}

void os_freeStackTrace(void *trace)
{
   struct stack_trace* os_trace = (struct stack_trace*)trace;

   if (!trace)
   {  // May be NULL, if kmalloc or vmalloc failed
      return;
   }

   kfree(os_trace->entries);
   kfree(os_trace);
}

/**
 * Print a stack trace
 *
 * @param trace    The stack trace to print
 * @param spaces   Insert 'spaces' white-spaces at the beginning of the line
 */
void os_printStackTrace(void* trace, int spaces)
{
   if (!trace)
   {  // Maybe NULL, if kmalloc or vmalloc failed
      return;
   }

   {
      struct stack_trace *stack_trace = trace;
#if defined(KERNEL_HAS_PRINT_STACK_TRACE)
      print_stack_trace(stack_trace, spaces);
#elif defined(KERNEL_HAS_STACK_TRACE_PRINT)
      stack_trace_print(stack_trace->entries, stack_trace->nr_entries, spaces);
#else
      (void) stack_trace;
#endif
   }
}


#else // no CONFIG_STACKTRACE or CONFIG_ARCH_STACKWALK enabled => nothing to do at all

void* os_saveStackTrace(void)
{
   return NULL;
}

void os_printStackTrace(void* trace, int spaces)
{
   printk_fhgfs(KERN_INFO, "Kernel without stack trace support!\n");
   return;
}

void os_freeStackTrace(void* trace)
{
   return;
}

#endif // CONFIG_STACKTRACE && !CONFIG_ARCH_STACKWALK

#endif // BEEGFS_DEBUG


