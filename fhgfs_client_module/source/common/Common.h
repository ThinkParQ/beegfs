#ifndef COMMON_H_
#define COMMON_H_

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/sched.h> /* for TASK_COMM_LEN */
#include <linux/string.h>
#include <linux/time.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <stdarg.h>
#include <linux/types.h>
#include <linux/stddef.h>
//#include <linux/compiler.h>
#include <asm/div64.h>

#include <common/FhgfsTypes.h>
#include <os/OsDeps.h>


/**
 * connection (response) timeouts in ms
 * note: be careful here, because servers not responding for >30secs under high load is nothing
 * unusual, so never use CONN_SHORT_TIMEOUT for IO-related operations.
 */
#define CONN_LONG_TIMEOUT     600000
#define CONN_MEDIUM_TIMEOUT    90000
#define CONN_SHORT_TIMEOUT     30000

#define MIN(a, b) \
   ( ( (a) < (b) ) ? (a) : (b) )
#define MAX(a, b) \
   ( ( (a) < (b) ) ? (b) : (a) )

#define SAFE_VFREE(p) \
   do{ if(p) {vfree(p); (p)=NULL;} } while(0)

#define SAFE_VFREE_NOSET(p) \
   do{ if(p) vfree(p); } while(0)

#define SAFE_KFREE(p) \
   do{ if(p) {kfree(p); (p)=NULL;} } while(0)

#define SAFE_KFREE_NOSET(p) \
   do{ if(p) kfree(p); } while(0)

#define SAFE_DESTRUCT(p, destructor) \
   do{if(p) {destructor(p); (p)=NULL;} } while(0)

#define SAFE_DESTRUCT_NOSET(p, destructor) \
   do{if(p) destructor(p); } while(0)


// typically used for optional out-args
#define SAFE_ASSIGN(destPointer, sourceValue) \
   do{ if(destPointer) {*(destPointer) = (sourceValue);} } while(0)


// module name

#ifndef BEEGFS_MODULE_NAME_STR
   #define BEEGFS_MODULE_NAME_STR          "beegfs"
#endif

#define BEEGFS_THREAD_NAME_PREFIX_STR   BEEGFS_MODULE_NAME_STR "_"

// a printk-version that adds the module name and comm name
#define printk_fhgfs(levelStr, fmtStr, ...) \
   printk(levelStr BEEGFS_MODULE_NAME_STR ": " "%s(%u): " fmtStr, current->comm, \
      (unsigned)current->pid, ## __VA_ARGS__)

// for interrupt handling routines (does not print "current")
#define printk_fhgfs_ir(levelStr, fmtStr, ...) \
   printk(levelStr BEEGFS_MODULE_NAME_STR ": " fmtStr, ## __VA_ARGS__)


// dumps stack in case of a buggy condition
#define BEEGFS_BUG_ON(condition, msgStr) \
   do { \
      if(unlikely(condition) ) { \
         printk_fhgfs(KERN_ERR, "%s:%d: BUG: %s (dumping stack...)\n", \
            __func__, __LINE__, msgStr); \
         dump_stack(); \
      } \
   } while(0)



#ifdef LOG_DEBUG_MESSAGES

#define printk_fhgfs_debug(levelStr, fmtStr, ...) \
   printk(levelStr BEEGFS_MODULE_NAME_STR ": " "%s(%u): " fmtStr, current->comm, \
      (unsigned)current->pid, ## __VA_ARGS__)


#define printk_fhgfs_ir_debug(levelStr, fmtStr, ...) \
   printk_fhgfs_ir(levelStr, fmtStr, ## __VA_ARGS__)

#define BEEGFS_BUG_ON_DEBUG(condition, msgStr)   BEEGFS_BUG_ON(condition, msgStr)

#else // !LOG_DEBUG_MESSAGES

#define printk_fhgfs_debug(levelStr, fmtStr, ...) \
   do { /* nothing */ } while(0)

#define printk_fhgfs_ir_debug(levelStr, fmtStr, ...) \
   do { /* nothing */ } while(0)

#define BEEGFS_BUG_ON_DEBUG(condition, msgStr) \
   do { /* nothing */ } while(0)

#endif // LOG_DEBUG_MESSAGES

#ifdef BEEGFS_OPENTK_LOG_CONN_ERRORS
   #define printk_fhgfs_connerr(levelStr, fmtStr, ...) \
      printk_fhgfs(levelStr, fmtStr, ## __VA_ARGS__)
#else
   #define printk_fhgfs_connerr(levelStr, fmtStr, ...) /* logging of conn errors disabled */
#endif // BEEGFS_OPENTK_LOG_CONN_ERRORS


// this macro mutes warnings about unused variables
#define IGNORE_UNUSED_VARIABLE(a)   do{ if( ((long)a)==1) {} } while(0)

// this macro mutes warnings about unsused variables that are only used in debug build
#ifdef BEEGFS_DEBUG
#define IGNORE_UNUSED_DEBUG_VARIABLE(a)   do{ /* do nothing */ } while(0)
#else
#define IGNORE_UNUSED_DEBUG_VARIABLE(a)   do{ if( ((long)a)==1) {} } while(0)
#endif

/* wrappers for get_fs()/set_fs() */
#define ACQUIRE_PROCESS_CONTEXT(fs_varname) \
   do { fs_varname = get_fs(); set_fs(KERNEL_DS); } while(0)
#define RELEASE_PROCESS_CONTEXT(fs_varname) \
   set_fs(fs_varname)


// get effective ID of current FS user/group access
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,29)
   #define currentFsUserID       (current->fsuid)
   #define currentFsGroupID      (current->fsgid)
#else
   #define currentFsUserID       current_fsuid()
   #define currentFsGroupID      current_fsgid()
#endif

/* Defined by <linux/include/linux/uidgid.h> and already included by one of the headers, so
 * no KernelFeatureDetection.mk detection required.
 * Note: Not in OsCompat.h, as OsCompat depends on Common.h. */
#ifndef _LINUX_UIDGID_H
   typedef unsigned kuid_t;
   typedef unsigned kgid_t;

   #define from_kuid(a, b) (b)
   #define from_kgid(a, b) (b)
   #define make_kuid(a, b) (b)
   #define make_kgid(a, b) (b)
#endif

#ifndef swap
   #define swap(a, b) do { typeof(a) __tmp = (a); (a) = (b); (b) = __tmp; } while (0)
#endif

static inline kuid_t FhgfsCommon_getCurrentKernelUserID(void);
static inline kgid_t FhgfsCommon_getCurrentKernelGroupID(void);
static inline unsigned FhgfsCommon_getCurrentUserID(void);
static inline unsigned FhgfsCommon_getCurrentGroupID(void);


kuid_t FhgfsCommon_getCurrentKernelUserID(void)
{
   return currentFsUserID;
}

kgid_t FhgfsCommon_getCurrentKernelGroupID(void)
{
   return currentFsGroupID;
}

unsigned FhgfsCommon_getCurrentUserID(void)
{
   kuid_t kernelUID = FhgfsCommon_getCurrentKernelUserID();

   return from_kuid(&init_user_ns, kernelUID);
}

unsigned FhgfsCommon_getCurrentGroupID(void)
{
   kgid_t kernelUID = FhgfsCommon_getCurrentKernelGroupID();

   return from_kgid(&init_user_ns, kernelUID);
}


/*
 * Debug definitions:
 * - LOG_DEBUG_MESSAGES: Enables logging of some extra debug messages that will not be
 *    available otherwise.
 * - DEBUG_REFCOUNT: Enables debugging of ObjectReferencer::refCount. Error messages will
 *    be logged if refCount is less than zero.
 */


#endif /*COMMON_H_*/
