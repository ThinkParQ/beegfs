#ifndef _fault_inject_fault_inject_h_dBK5RbRnOrGDnIW5Y7zvnv
#define _fault_inject_fault_inject_h_dBK5RbRnOrGDnIW5Y7zvnv

#include <linux/types.h>

#if defined(BEEGFS_DEBUG) && defined(CONFIG_FAULT_INJECTION)

#include <linux/fault-inject.h>

#ifndef CONFIG_DEBUG_FS
#error debugfs required for beegfs fault injection
#endif
#ifndef CONFIG_FAULT_INJECTION_DEBUG_FS
#error CONFIG_FAULT_INJECTION_DEBUG_FS required for beegfs fault injection
#endif

#define BEEGFS_SHOULD_FAIL(name, size) should_fail(&beegfs_fault_ ## name, size)
#define BEEGFS_DEFINE_FAULT(name) extern struct fault_attr beegfs_fault_ ## name

BEEGFS_DEFINE_FAULT(readpage);
BEEGFS_DEFINE_FAULT(writepage);

BEEGFS_DEFINE_FAULT(write_force_cache_bypass);
BEEGFS_DEFINE_FAULT(read_force_cache_bypass);

BEEGFS_DEFINE_FAULT(commkit_sendheader_timeout);
BEEGFS_DEFINE_FAULT(commkit_polltimeout);
BEEGFS_DEFINE_FAULT(commkit_recvheader_timeout);

BEEGFS_DEFINE_FAULT(commkit_readfile_receive_timeout);
BEEGFS_DEFINE_FAULT(commkit_writefile_senddata_timeout);

extern bool beegfs_fault_inject_init(void);
extern void beegfs_fault_inject_release(void);

#else // BEEGFS_DEBUG

#define BEEGFS_SHOULD_FAIL(name, size) (false)

static inline bool beegfs_fault_inject_init(void)
{
   return true;
}

static inline void beegfs_fault_inject_release(void)
{
}

#endif // BEEGFS_DEBUG

#endif
