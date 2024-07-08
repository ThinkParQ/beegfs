#include <app/log/Logger.h>
#include <app/App.h>
#include <app/config/Config.h>
#include <filesystem/ProcFs.h>
#include <os/OsCompat.h>
#include <common/storage/StorageDefinitions.h>
#include <common/toolkit/MetadataTk.h>
#include <common/Common.h>
#include <components/worker/RWPagesWork.h>
#include <net/filesystem/FhgfsOpsRemoting.h>
#include "FhgfsOps_versions.h"
#include "FhgfsOpsSuper.h"
#include "FhgfsOpsInode.h"
#include "FhgfsOpsFile.h"
#include "FhgfsOpsDir.h"
#include "FhgfsOpsPages.h"
#include "FhgfsOpsExport.h"
#include "FhgfsXAttrHandlers.h"


static int  __FhgfsOps_initApp(struct super_block* sb, char* rawMountOptions);
static void __FhgfsOps_uninitApp(App* app);

static int  __FhgfsOps_constructFsInfo(struct super_block* sb, void* rawMountOptions);
static void __FhgfsOps_destructFsInfo(struct super_block* sb);


/* read-ahead size is limited by BEEGFS_DEFAULT_READAHEAD_PAGES, so this is the maximum already going
 * to to the server. 32MiB read-head also seems to be a good number. It still may be reduced by
 * setting /sys/class/bdi/fhgfs-${number}/read_ahead_kb */
#define BEEGFS_DEFAULT_READAHEAD_PAGES BEEGFS_MAX_PAGE_LIST_SIZE

static struct file_system_type fhgfs_fs_type =
{
   .name       = BEEGFS_MODULE_NAME_STR,
   .owner      = THIS_MODULE,
   .kill_sb    = FhgfsOps_killSB,
   //.fs_flags   = FS_BINARY_MOUNTDATA, // not required currently

#ifdef KERNEL_HAS_GET_SB_NODEV
   .get_sb     = FhgfsOps_getSB,
#else
   .mount      = FhgfsOps_mount, // basically the same thing as get_sb before
#endif
};

static struct super_operations fhgfs_super_ops =
{
   .statfs        = FhgfsOps_statfs,
   .alloc_inode   = FhgfsOps_alloc_inode,
   .destroy_inode = FhgfsOps_destroy_inode,
   .drop_inode    = generic_drop_inode,
   .put_super     = FhgfsOps_putSuper,
   .show_options  = FhgfsOps_showOptions,
};

/**
 * Creates and initializes the per-mount application object.
 */
int __FhgfsOps_initApp(struct super_block* sb, char* rawMountOptions)
{
   MountConfig* mountConfig;
   bool parseRes;
   App* app;
   int appRes;

   // create mountConfig (parse from mount options)
   mountConfig = MountConfig_construct();

   parseRes = MountConfig_parseFromRawOptions(mountConfig, rawMountOptions);
   if(!parseRes)
   {
      MountConfig_destruct(mountConfig);
      return APPCODE_INVALID_CONFIG;
   }

   //printk_fhgfs(KERN_INFO, "Initializing App...\n"); // debug in

   app = FhgfsOps_getApp(sb);
   App_init(app, mountConfig);

   appRes = App_run(app);

   if(appRes != APPCODE_NO_ERROR)
   { // error occurred => clean up
      printk_fhgfs_debug(KERN_INFO, "Stopping App...\n");

      App_stop(app);

      printk_fhgfs_debug(KERN_INFO, "Cleaning up...\n");

      App_uninit(app);

      printk_fhgfs_debug(KERN_INFO, "App unitialized.\n");

      return appRes;
   }

   ProcFs_createEntries(app);

   return appRes;
}

/**
 * Stops and destroys the per-mount application object.
 */
void __FhgfsOps_uninitApp(App* app)
{
   App_stop(app);

   /* note: some of the procfs entries (e.g. remove_node) won't work anymore after app components
      have been stopped, but others are still useful for finding reasons why app stop is delayed
      in some cases (so we remove procfs after App_stop() ). */

   ProcFs_removeEntries(app);

   App_uninit(app);
}

int FhgfsOps_registerFilesystem(void)
{
   return register_filesystem(&fhgfs_fs_type);
}

int FhgfsOps_unregisterFilesystem(void)
{
   return unregister_filesystem(&fhgfs_fs_type);
}

/**
 * Initialize sb->s_fs_info
 *
 * @return 0 on success, negative linux error code otherwise
 */
int __FhgfsOps_constructFsInfo(struct super_block* sb, void* rawMountOptions)
{
   int res;
   int appRes;
   App* app;
   Logger* log;



#if defined(KERNEL_HAS_SB_BDI) && !defined(KERNEL_HAS_SUPER_SETUP_BDI_NAME)
   struct backing_dev_info* bdi;
#endif

   // use kzalloc to also zero the bdi
   FhgfsSuperBlockInfo* sbInfo = kzalloc(sizeof(FhgfsSuperBlockInfo), GFP_KERNEL);
   if (!sbInfo)
   {
      printk_fhgfs_debug(KERN_INFO, "Failed to allocate memory for FhgfsSuperBlockInfo");
      sb->s_fs_info = NULL;
      return -ENOMEM;
   }

   sb->s_fs_info = sbInfo;

   appRes = __FhgfsOps_initApp(sb, rawMountOptions);
   if(appRes)
   {
      printk_fhgfs_debug(KERN_INFO, "Failed to initialize App object");
      res = -EINVAL;
      goto outFreeSB;
   }

   app = FhgfsOps_getApp(sb);
   log = App_getLogger(app);
   IGNORE_UNUSED_VARIABLE(log);

#if defined(KERNEL_HAS_SB_BDI)

   #if defined(KERNEL_HAS_SUPER_SETUP_BDI_NAME) && !defined(KERNEL_HAS_BDI_SETUP_AND_REGISTER)
   {
      static atomic_long_t bdi_seq = ATOMIC_LONG_INIT(0);

      res = super_setup_bdi_name(sb, BEEGFS_MODULE_NAME_STR "-%ld",
            atomic_long_inc_return(&bdi_seq));
   }
   #else
      bdi = &sbInfo->bdi;

      /* NOTE: The kernel expects a fully initialized bdi structure, so at a minimum it has to be
       *       allocated by kzalloc() or memset(bdi, 0, sizeof(*bdi)).
       *       we don't set the congest_* callbacks (like every other filesystem) because those are
       *       intended for dm and md.
       */
      bdi->ra_pages = BEEGFS_DEFAULT_READAHEAD_PAGES;

      #if defined(KERNEL_HAS_BDI_CAP_MAP_COPY) 
         res = bdi_setup_and_register(bdi, BEEGFS_MODULE_NAME_STR, BDI_CAP_MAP_COPY);
      #else
         res = bdi_setup_and_register(bdi, BEEGFS_MODULE_NAME_STR);
      #endif
   #endif

   if (res)
   {
      Logger_logFormatted(log, 2, __func__, "Failed to init super-block (bdi) information: %d",
         res);
      __FhgfsOps_uninitApp(app);
      goto outFreeSB;
   }
#endif

   // set root inode attribs to uninit'ed

   FhgfsOps_setHasRootEntryInfo(sb, false);
   FhgfsOps_setIsRootInited(sb, false);


   printk_fhgfs(KERN_INFO, "BeeGFS mount ready.\n");

   return 0; // all ok, res should be 0 here

outFreeSB:
   kfree(sbInfo);
   sb->s_fs_info = NULL;

   return res;
}

/**
 * Unitialize the entire sb->s_fs_info object
 */
void __FhgfsOps_destructFsInfo(struct super_block* sb)
{
   /* sb->s_fs_info might be NULL if __FhgfsOps_constructFsInfo() failed */
   if (sb->s_fs_info)
   {
      App* app = FhgfsOps_getApp(sb);

//call destroy iff not initialised/registered by super_setup_bdi_name
#if defined(KERNEL_HAS_SB_BDI)

#if !defined(KERNEL_HAS_SUPER_SETUP_BDI_NAME) || defined(KERNEL_HAS_BDI_SETUP_AND_REGISTER)
      struct backing_dev_info* bdi = FhgfsOps_getBdi(sb);

      bdi_destroy(bdi);
#endif

#endif

      __FhgfsOps_uninitApp(app);

      kfree(sb->s_fs_info);
      sb->s_fs_info = NULL;

      printk_fhgfs(KERN_INFO, "BeeGFS unmounted.\n");
   }
}

/**
 * Fill the file system superblock (vfs object)
 */
int FhgfsOps_fillSuper(struct super_block* sb, void* rawMountOptions, int silent)
{
   App* app = NULL;
   Config* cfg = NULL;

   struct inode* rootInode;
   struct dentry* rootDentry;
   struct kstat kstat;
   EntryInfo entryInfo;

   FhgfsIsizeHints iSizeHints;

   // init per-mount app object

   if(__FhgfsOps_constructFsInfo(sb, rawMountOptions) )
      return -ECANCELED;

   app = FhgfsOps_getApp(sb);
   cfg = App_getConfig(app);

   // set up super block data

   sb->s_maxbytes = MAX_LFS_FILESIZE;
   sb->s_blocksize = PAGE_SIZE;
   sb->s_blocksize_bits = PAGE_SHIFT;
   sb->s_magic = BEEGFS_MAGIC;
   sb->s_op = &fhgfs_super_ops;
   sb->s_time_gran = 1000000000; // granularity of c/m/atime in ns
#ifdef KERNEL_HAS_SB_NODIRATIME
   sb->s_flags |= SB_NODIRATIME;
#else
   sb->s_flags |= MS_NODIRATIME;
#endif

   if (Config_getSysXAttrsEnabled(cfg ) )
      sb->s_xattr = fhgfs_xattr_handlers_noacl; // handle only user xattrs

#ifdef KERNEL_HAS_GET_ACL
   if (Config_getSysACLsEnabled(cfg) )
   {
      sb->s_xattr = fhgfs_xattr_handlers; // replace with acl-capable xattr handlers
#ifdef SB_POSIXACL
      sb->s_flags |= SB_POSIXACL;
#else
      sb->s_flags |= MS_POSIXACL;
#endif
   }
#endif // KERNEL_HAS_GET_ACL
   if (Config_getSysXAttrsCheckCapabilities(cfg) != CHECKCAPABILITIES_Always)
   #if defined(SB_NOSEC)
      sb->s_flags |= SB_NOSEC;
   #else
      sb->s_flags |= MS_NOSEC;
   #endif

   /* MS_ACTIVE is rather important as it marks the super block being successfully initialized and
    * allows the vfs to keep important inodes in the cache. However, it seems it is already
    * initialized in vfs generic mount functions.
      sb->s_flags |= MS_ACTIVE; // used in iput_final()  */

   // NFS kernel export is probably not worth the backport efforts for kernels before 2.6.29
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29)
   sb->s_export_op = &fhgfs_export_ops;
#endif

#if defined(KERNEL_HAS_SB_BDI)
   sb->s_bdi = FhgfsOps_getBdi(sb);
#endif

   // init root inode

   memset(&kstat, 0, sizeof(struct kstat) );

   kstat.ino = BEEGFS_INODE_ROOT_INO;
   kstat.mode = S_IFDIR | 0777; // allow access for everyone
   kstat.atime = kstat.mtime = kstat.ctime = current_fs_time(sb);
   kstat.uid = current_fsuid();
   kstat.gid = current_fsgid();
   kstat.blksize = Config_getTuneInodeBlockSize(cfg);
   kstat.nlink = 1;

   // root entryInfo is always updated when someone asks for it (so we just set dummy values here)
   EntryInfo_init(&entryInfo, NodeOrGroup_fromGroup(0), StringTk_strDup(""), StringTk_strDup(""),
      StringTk_strDup(""), DirEntryType_DIRECTORY, 0);

   rootInode = __FhgfsOps_newInode(sb, &kstat, 0, &entryInfo, &iSizeHints);
   if(!rootInode || IS_ERR(rootInode) )
   {
      __FhgfsOps_destructFsInfo(sb);
      return IS_ERR(rootInode) ? PTR_ERR(rootInode) : -ENOMEM;
   }

   rootDentry = d_make_root(rootInode);
   if(!rootDentry)
   {
      __FhgfsOps_destructFsInfo(sb);
      return -ENOMEM;
   }

#ifdef KERNEL_HAS_S_D_OP
   // linux 2.6.38 switched from individual per-dentry to defaul superblock d_ops.
   /* note: Only set default dentry operations here, as we don't want those OPs set for the root
    * dentry. In fact, setting as before would only slow down everything a bit, due to
    * useless revalidation of our root dentry. */
   sb->s_d_op = &fhgfs_dentry_ops;
#endif // KERNEL_HAS_S_D_OP

   rootDentry->d_time = jiffies;
   sb->s_root = rootDentry;

   return 0;
}

/*
 * Called by FhgfsOps_killSB()->kill_anon_super()->generic_shutdown_super()
 */
void FhgfsOps_putSuper(struct super_block* sb)
{
   if (sb->s_fs_info)
   {
      App* app = FhgfsOps_getApp(sb);

      if(app)
         __FhgfsOps_destructFsInfo(sb);
   }
}

void FhgfsOps_killSB(struct super_block* sb)
{
   App* app = FhgfsOps_getApp(sb);

   if (app) // might be NULL on unsuccessful mount attempt
      App_setConnRetriesEnabled(app, false); // faster umount on communication errors

   RWPagesWork_flushWorkQueue();

#if defined(KERNEL_HAS_SB_BDI) && LINUX_VERSION_CODE < KERNEL_VERSION(4,1,0)
   /**
    * s_fs_info might be NULL
    */
   if (likely(sb->s_fs_info) )
   {
      struct backing_dev_info* bdi = FhgfsOps_getBdi(sb);

      bdi_unregister(bdi);
   }
#endif

   kill_anon_super(sb);
}


#ifdef KERNEL_HAS_SHOW_OPTIONS_DENTRY
extern int FhgfsOps_showOptions(struct seq_file* sf, struct dentry* dentry)
{
   struct super_block* super = dentry->d_sb;
#else
extern int FhgfsOps_showOptions(struct seq_file* sf, struct vfsmount* vfs)
{
   struct super_block* super = vfs->mnt_sb;
#endif

   App* app = FhgfsOps_getApp(super);
   MountConfig* mountConfig = App_getMountConfig(app);

   MountConfig_showOptions(mountConfig, sf);

   return 0;
}

