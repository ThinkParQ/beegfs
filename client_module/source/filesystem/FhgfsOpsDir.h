#ifndef FHGFSOPSDIR_H_
#define FHGFSOPSDIR_H_

#include <app/App.h>
#include <common/threading/Mutex.h>
#include <common/Common.h>
#include <common/toolkit/Time.h>
#include <net/filesystem/FhgfsOpsRemoting.h>
#include <filesystem/FsDirInfo.h>
#include <filesystem/FsFileInfo.h>
#include <toolkit/NoAllocBufferStore.h>
#include <common/toolkit/MetadataTk.h>
#include <common/Common.h>
#include "FhgfsOps_versions.h"
#include "FhgfsOpsInode.h"

#include <linux/dcache.h>


extern struct dentry_operations fhgfs_dentry_ops;


#ifndef KERNEL_HAS_ATOMIC_OPEN
   extern int FhgfsOps_revalidateIntent(struct dentry* dentry, struct nameidata* nameidata);
#else
   extern int FhgfsOps_revalidateIntent(struct dentry* dentry, unsigned flags);
#endif // LINUX_VERSION_CODE

#ifndef KERNEL_HAS_D_DELETE_CONST_ARG
   extern int FhgfsOps_deleteDentry(struct dentry* dentry);
#else
   extern int FhgfsOps_deleteDentry(const struct dentry* dentry);
#endif // LINUX_VERSION_CODE


extern char* __FhgfsOps_pathResolveToStoreBuf(NoAllocBufferStore* bufStore,
   struct dentry* dentry, char** outStoreBuf);

#endif /* FHGFSOPSDIR_H_ */
