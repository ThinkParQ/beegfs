#ifndef FHGFSOPSDIR_H_
#define FHGFSOPSDIR_H_

#include <linux/dcache.h>

#include <toolkit/NoAllocBufferStore.h>


extern struct dentry_operations fhgfs_dentry_ops;

extern char* __FhgfsOps_pathResolveToStoreBuf(NoAllocBufferStore* bufStore,
   struct dentry* dentry, char** outStoreBuf);

#endif /* FHGFSOPSDIR_H_ */
