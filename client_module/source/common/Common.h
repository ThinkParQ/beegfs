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
#include <linux/uaccess.h>

#ifdef KERNEL_HAS_LINUX_FILELOCK_H
#include <linux/filelock.h>
#endif

#ifdef KERNEL_HAS_LINUX_STDARG_H
#include <linux/stdarg.h>
#else
#include <stdarg.h>
#endif

#include <linux/types.h>
#include <linux/stddef.h>
#include <linux/cred.h>
#include <asm/div64.h>

#ifdef KERNEL_HAS_SCHED_SIG_H
#include <linux/sched/signal.h>
#endif

#include <common/FhgfsTypes.h>
#include <os/OsDeps.h>

/**
 * NOTE: These timeouts can now be overridden by the connMessagingTimeouts
 * option in the configuration file. If that option is unset or set to <=0, we
 * still default to these constants.
 */
#define CONN_LONG_TIMEOUT     600000
#define CONN_MEDIUM_TIMEOUT    90000
#define CONN_SHORT_TIMEOUT     30000

#ifndef MIN
#define MIN(a, b) \
   ( ( (a) < (b) ) ? (a) : (b) )
#endif
#ifndef MAX
#define MAX(a, b) \
   ( ( (a) < (b) ) ? (b) : (a) )
#endif

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



////////////////////////////////////////////////////////
/* set_fs() / get_fs() macro hackery.
 *
 * set_fs() and get_fs() have disappeared with Linux kernel 5.10.
 * For older kernels, we employ some macros to make their use less of a hassle.
 */
////////////////////////////////////////////////////////

#define BEEGFS_CONCAT_(x, y) x ## y
#define BEEGFS_CONCAT(x, y) BEEGFS_CONCAT_(x, y)
#define BEEGFS_UNIQUE_NAME(prefix) BEEGFS_CONCAT(prefix, __LINE__)


// Lifted from Linux 5.10
#if __has_attribute(__fallthrough__)
#define BEEGFS_FALLTHROUGH __attribute__((__fallthrough__))
#else
#define BEEGFS_FALLTHROUGH do {} while (0) /* FALLTHROUGH */
#endif


/* Preprocessor hack to add statements that are executed on scope cleanup.
 * A for-loop that runs exactly 1 time is misused to execute the cleanup
 * statement. An assertion ensures that we didn't break from the inner loop,
 * to ensure the cleanup statement is executed. */

#define BEEGFS_FOR_SCOPE_(begin_stmt, end_stmt, name) \
   for (int name = 0; !name; ({BUG_ON(!name);})) \
      for (begin_stmt; !name++; end_stmt)

#define BEEGFS_FOR_SCOPE(begin_stmt, end_stmt) \
   BEEGFS_FOR_SCOPE_(begin_stmt, end_stmt, BEEGFS_UNIQUE_NAME(scope))

#ifdef KERNEL_HAS_GET_FS

static inline mm_segment_t BEEGFS_BEGIN_PROCESS_CONTEXT(void)
{
   mm_segment_t out = get_fs();
   set_fs(KERNEL_DS);
   return out;
}

static inline void BEEGFS_END_PROCESS_CONTEXT(mm_segment_t *backup)
{
   set_fs(*backup);
}

#define WITH_PROCESS_CONTEXT \
   BEEGFS_FOR_SCOPE( \
      mm_segment_t segment = BEEGFS_BEGIN_PROCESS_CONTEXT(), \
      BEEGFS_END_PROCESS_CONTEXT(&segment))

#else

#define WITH_PROCESS_CONTEXT

#endif

////////////////////////////////////////////////////////





// in 4.13 wait_queue_t got renamed to wait_queue_entry_t
#if defined(KERNEL_HAS_WAIT_QUEUE_ENTRY_T)
 typedef wait_queue_entry_t wait_queue_t;
#endif

#if defined(KERNEL_HAS_64BIT_TIMESTAMPS)
static inline struct timespec64 current_fs_time(struct super_block *sb)
{
   struct timespec64 now;
#if defined(KERNEL_HAS_KTIME_GET_COARSE_REAL_TS64)
   ktime_get_coarse_real_ts64(&now);
   return now;
#elif defined(KERNEL_HAS_KTIME_GET_REAL_TS64)
   ktime_get_real_ts64(&now);
   return timespec64_trunc(now, sb->s_time_gran);
#else
   now = current_kernel_time64();
   return timespec64_trunc(now, sb->s_time_gran);
#endif
}
#elif !defined(KERNEL_HAS_CURRENT_FS_TIME)
static inline struct timespec current_fs_time(struct super_block *sb)
{
   struct timespec now = current_kernel_time();
   return timespec_trunc(now, sb->s_time_gran);
}
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

#undef BEEGFS_RDMA
#ifndef BEEGFS_NO_RDMA
#if defined(CONFIG_INFINIBAND) || defined(CONFIG_INFINIBAND_MODULE)
#define BEEGFS_RDMA 1
#endif
#endif

static inline unsigned FhgfsCommon_getCurrentUserID(void);
static inline unsigned FhgfsCommon_getCurrentGroupID(void);


unsigned FhgfsCommon_getCurrentUserID(void)
{
   return from_kuid(&init_user_ns, current_fsuid());
}

unsigned FhgfsCommon_getCurrentGroupID(void)
{
   return from_kgid(&init_user_ns, current_fsgid());
}

// Helper function for getting file pointer
static inline struct file * FhgfsCommon_getFileLock(struct file_lock *fileLock)
{
#if defined(KERNEL_HAS_FILE_LOCK_CORE)
    return fileLock->c.flc_file;
#else
    return fileLock->fl_file;
#endif
}
// Helper function to get PID from file lock
static inline pid_t FhgfsCommon_getFileLockPID(struct file_lock *fileLock)
{
#if defined(KERNEL_HAS_FILE_LOCK_CORE)
    return fileLock->c.flc_pid;
#else
    return fileLock->fl_pid;
#endif
}

// Helper function to get lock type
static inline unsigned char FhgfsCommon_getFileLockType(struct file_lock *flock)
{
#if defined(KERNEL_HAS_FILE_LOCK_CORE)
    return flock->c.flc_type;
#else
    return flock->fl_type;
#endif
}

// Helper function to get lock flags
static inline unsigned int FhgfsCommon_getFileLockFlags(struct file_lock *fileLock)
{
#if defined(KERNEL_HAS_FILE_LOCK_CORE)
    return fileLock->c.flc_flags;
#else
    return fileLock->fl_flags;
#endif
}


/*
 * Debug definitions:
 * - LOG_DEBUG_MESSAGES: Enables logging of some extra debug messages that will not be
 *    available otherwise.
 * - DEBUG_REFCOUNT: Enables debugging of ObjectReferencer::refCount. Error messages will
 *    be logged if refCount is less than zero.
 */

/**
 * BEEGFS_KFREE_LIST() - destroys and kfree()s all element of a list, leaving an empty list
 *
 * expands to ``BEEGFS_KFREE_LIST_DTOR(List, ElemType, Member, (void))``, ie no destructor is
 * called.
 */
#define BEEGFS_KFREE_LIST(List, ElemType, Member) \
   BEEGFS_KFREE_LIST_DTOR(List, ElemType, Member, (void))
/**
 * BEEGFS_KFREE_LIST_DTOR() - destroys and kfree()s all element of a list, leaving an empty list
 * @List the &struct list_head to free
 * @ElemType type of elements contained in the list
 * @Member name of the &struct list_head in @ElemType that links to the next element of the list
 * @Dtor for each element of the list, ``Dtor(entry)`` is evaluated before the entry is freed
 */
#define BEEGFS_KFREE_LIST_DTOR(List, ElemType, Member, Dtor) \
   do { \
      ElemType* entry; \
      ElemType* n; \
      list_for_each_entry_safe(entry, n, List, Member) \
      { \
         Dtor(entry); \
         kfree(entry); \
      } \
      INIT_LIST_HEAD(List); \
   } while (0)

/**
 * Generic key comparison function for integer types and pointers, to be used by the
 * RBTREE_FUNCTIONS as KeyCmp.
 */
#define BEEGFS_RB_KEYCMP_LT_INTEGRAL(lhs, rhs) \
   ( lhs < rhs ? -1 : (lhs == rhs ? 0 : 1) )

/**
 * BEEGFS_KFREE_RBTREE() - destroys and kfree()s all elements an rbtree
 *
 * expands to ``BEEGFS_KFREE_RBTREE_DTOR(TreeRoot, ElemType, Member, (void))``, ie no destructor is
 * called.
 */
#define BEEGFS_KFREE_RBTREE(TreeRoot, ElemType, Member) \
   BEEGFS_KFREE_RBTREE_DTOR(TreeRoot, ElemType, Member, (void))
/**
 * BEEGFS_KFREE_RBTREE_DTOR() - destroys and kfree()s all elements an rbtree, leaving an empty tree
 * @TreeRoot &struct rb_root to free
 * @ElemType type of elements contained in the tree
 * @Member name of the &struct rb_node in @ElemType that links to further entries in the tree
 * @Dtor for each element of the tree, ``Dtor(elem)`` is evaluated before the entry is freed
 */
#define BEEGFS_KFREE_RBTREE_DTOR(TreeRoot, ElemType, Member, Dtor) \
   do { \
      ElemType* pos; \
      ElemType* n; \
      rbtree_postorder_for_each_entry_safe(pos, n, TreeRoot, Member) \
      { \
         Dtor(pos); \
         kfree(pos); \
      } \
      *(TreeRoot) = RB_ROOT; \
   } while (0)

/**
 * BEEGFS_RBTREE_FUNCTIONS() - defines a default set of rbtree functions
 * @Access access modifier of generated functions
 * @NS namespace prefix for all defined functions
 * @RootTy type of the struct that contains the tree we are building functions for
 * @RootNode name of the &struct rb_root of our tree in @RootTy
 * @KeyTy type of the trees sort key
 * @ElemTy type of the tree element. must contain a field of type @KeyTy
 * @ElemKey name of the key member in @ElemTy (must be of type @KeyTy)
 * @ElemNode node of the &struct rb_node of our tree in @ElemTy
 * @KeyCmp function or macro used to compare to @KeyTy values for tree ordering
 *
 * This macro declares a number of functions with names prefixed by @NS:
 *
 *  * ``Access ElemTy* NS##_find(const RootTy*, KeyTy key)`` finds the @ElemTy with key ``key`` and
 *    returns it. If none exists, %NULL is returned.
 *  * ``Access bool NS##_insert(RootTy*, ElemTy* data)`` inserts the element ``data`` into the tree
 *    if no  element with the same key exists and return %true. If an element with the same key does
 *    exist, returns %false.
 *  * ``Access ElemTy* NS##_insertOrReplace(RootTy*, ElemTy* data)`` inserts ``data`` into the tree
 *    if no element with the same key exists or replaced the existing item with the same key. If no
 *    item existed %NULL is returned, otherwise the pointer to the previous element is returned.
 *    This is analogous to how std::map::operator[] works in C++.
 *  * ``access void NS##_erase(RootTy*, ElemTy* data)`` removes ``data`` from the tree. ``data`` is
 *    not freed or otherwise processed, this function only removes the item and rebalances the tree
 *    if necessary.
 */
#define BEEGFS_RBTREE_FUNCTIONS(Access, NS, RootTy, RootNode, KeyTy, ElemTy, ElemKey, ElemNode, \
      KeyCmp) \
   __attribute__((unused)) \
   Access ElemTy* NS##_find(const RootTy* root, KeyTy key) \
   { \
      struct rb_node* node = root->RootNode.rb_node; \
      while (node) \
      { \
         ElemTy* data = rb_entry(node, ElemTy, ElemNode); \
         int cmp = KeyCmp(key, data->ElemKey); \
         \
         if (cmp < 0) \
            node = node->rb_left; \
         else if (cmp > 0) \
            node = node->rb_right; \
         else \
            return data; \
      } \
      return NULL; \
   } \
   __attribute__((unused)) \
   Access bool NS##_insert(RootTy* root, ElemTy* data) \
   { \
      struct rb_node** new = &root->RootNode.rb_node; \
      struct rb_node* parent = NULL; \
      while (*new) \
      { \
         ElemTy* cur = container_of(*new, ElemTy, ElemNode); \
         int cmp = KeyCmp(data->ElemKey, cur->ElemKey); \
         \
         parent = *new; \
         if (cmp < 0) \
            new = &(*new)->rb_left; \
         else if (cmp > 0) \
            new = &(*new)->rb_right; \
         else \
            return false; \
      } \
      rb_link_node(&data->ElemNode, parent, new); \
      rb_insert_color(&data->ElemNode, &root->RootNode); \
      return true; \
   } \
   __attribute__((unused)) \
   Access ElemTy* NS##_insertOrReplace(RootTy* root, ElemTy* data) \
   { \
      ElemTy* existing; \
      if (NS##_insert(root, data)) \
         return NULL; \
      \
      existing = NS##_find(root, data->ElemKey); \
      rb_replace_node(&existing->ElemNode, &data->ElemNode, &root->RootNode); \
      return existing; \
   } \
   __attribute__((unused)) \
   Access void NS##_erase(RootTy* root, ElemTy* data) \
   { \
      rb_erase(&data->ElemNode, &root->RootNode); \
   }

#define BEEGFS_RBTREE_FOR_EACH_ENTRY(Pos, Root, Node) \
   for (Pos = rb_entry_safe(rb_first(Root), typeof(*Pos), Node); \
         Pos; \
         Pos = rb_entry_safe(rb_next(&Pos->Node), typeof(*Pos), Node))

/* version number of both the network protocol and the on-disk data structures that are versioned.
 * must be kept in sync with userspace. */
#define BEEGFS_DATA_VERSION ((uint32_t)0)

#endif /*COMMON_H_*/
