#include <app/App.h>
#include <common/toolkit/SocketTk.h>
#include <common/Common.h>
#include <fault-inject/fault-inject.h>
#include <filesystem/FhgfsOpsSuper.h>
#include <filesystem/FhgfsOpsInode.h>
#include <filesystem/FhgfsOpsPages.h>
#include <filesystem/FhgfsOpsFileNative.h>
#include <filesystem/ProcFs.h>
#include <components/worker/RWPagesWork.h>
#include <net/filesystem/FhgfsOpsCommKit.h>
#include <net/filesystem/FhgfsOpsRemoting.h>

#define BEEGFS_LICENSE "GPL v2"

static int __init init_fhgfs_client(void)
{
#define fail_to(target, msg) \
   do { \
      printk_fhgfs(KERN_WARNING, msg "\n"); \
      goto target; \
   } while (0)

   if (!beegfs_fault_inject_init() )
      fail_to(fail_fault, "could not register fault-injection debugfs dentry");

   if (!beegfs_native_init() )
      fail_to(fail_native, "could not allocate emergency pools");

   if (!FhgfsOpsCommKit_initEmergencyPools() )
      fail_to(fail_commkitpools, "could not allocate emergency pools");

   if (!SocketTk_initOnce() )
      fail_to(fail_socket, "SocketTk initialization failed");

   if (!FhgfsOps_initInodeCache() )
      fail_to(fail_inode, "Inode cache initialization failed");

   if (!RWPagesWork_initworkQueue() )
      fail_to(fail_rwpages, "Page work queue registration failed");

   if (!FhgfsOpsRemoting_initMsgBufCache() )
      fail_to(fail_msgbuf, "Message cache initialization failed");

   if (!FhgfsOpsPages_initPageListVecCache() )
      fail_to(fail_pagelists, "PageVec cache initialization failed");

   if (FhgfsOps_registerFilesystem() )
      fail_to(fail_register, "File system registration failed");

   ProcFs_createGeneralDir();

   printk_fhgfs(KERN_INFO, "File system registered. Type: %s. Version: %s\n",
      BEEGFS_MODULE_NAME_STR, App_getVersionStr() );

   return 0;

fail_register:
   FhgfsOpsPages_destroyPageListVecCache();
fail_pagelists:
   FhgfsOpsRemoting_destroyMsgBufCache();
fail_msgbuf:
   RWPagesWork_destroyWorkQueue();
fail_rwpages:
   FhgfsOps_destroyInodeCache();
fail_inode:
   SocketTk_uninitOnce();
fail_socket:
   FhgfsOpsCommKit_releaseEmergencyPools();
fail_commkitpools:
   beegfs_native_release();
fail_native:
   beegfs_fault_inject_release();
fail_fault:
   return -EPERM;
}

static void __exit exit_fhgfs_client(void)
{
   ProcFs_removeGeneralDir();
   BUG_ON(FhgfsOps_unregisterFilesystem() );
   FhgfsOpsPages_destroyPageListVecCache();
   FhgfsOpsRemoting_destroyMsgBufCache();
   RWPagesWork_destroyWorkQueue();
   FhgfsOps_destroyInodeCache();
   SocketTk_uninitOnce();
   FhgfsOpsCommKit_releaseEmergencyPools();
   beegfs_native_release();
   beegfs_fault_inject_release();

   printk_fhgfs(KERN_INFO, "BeeGFS client unloaded.\n");
}

module_init(init_fhgfs_client)
module_exit(exit_fhgfs_client)

MODULE_LICENSE(BEEGFS_LICENSE);
MODULE_DESCRIPTION("BeeGFS parallel file system client (https://www.beegfs.io)");
MODULE_AUTHOR("ThinkParQ GmbH");
MODULE_ALIAS("fs-" BEEGFS_MODULE_NAME_STR);
MODULE_VERSION(BEEGFS_VERSION);
