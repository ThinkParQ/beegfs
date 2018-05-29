#ifndef FhgfsOpsFileNative_h_bI2n6XRVSaWCQHxNdwQA0I
#define FhgfsOpsFileNative_h_bI2n6XRVSaWCQHxNdwQA0I

#include <linux/fs.h>

extern const struct file_operations fhgfs_file_native_ops;
extern const struct address_space_operations fhgfs_addrspace_native_ops;

extern bool beegfs_native_init(void);
extern void beegfs_native_release(void);

#endif
