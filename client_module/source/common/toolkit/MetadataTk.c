#include <app/App.h>
#include <app/log/Logger.h>
#include <common/net/message/NetMessage.h>
#include <common/storage/Metadata.h>
#include <common/storage/StorageDefinitions.h>
#include <common/storage/StorageErrors.h>
#include <common/toolkit/MessagingTk.h>
#include <nodes/NodeStoreEx.h>
#include <toolkit/NoAllocBufferStore.h>

#include "MetadataTk.h"

/**
 * Used to initialize struct CreateInfo. This function always should be used, to make sure,
 * values are not forgotten.
 *
 * Note: preferred meta/storage targets are automatically set to app's preferred meta/storage
 *       targets; keep that in mind when you're cleaning up.
 *
 * Resolve the creator's UID/GID for new objects based on the mount's ID mapping.
 *
 * For ≥ 6.3 kernels (ID-mapped mounts):
 *   Use the mnt_idmap passed from the VFS hook and translate the caller’s
 *   fsuid/fsgid into filesystem-visible numeric IDs using Fhgfs_uid_for_fs()
 *   and Fhgfs_gid_for_fs().
 *
 * For 5.15–6.2 kernels (userns mounts):
 *   Use mnt_userns to map fsuid/fsgid into user-visible IDs.
 *
 * For legacy kernels:
 *   Collapse IDs into init_user_ns.
 */
#if defined(KERNEL_HAS_IDMAPPED_MOUNTS)
void CreateInfo_init(App *app, struct mnt_idmap *idmap, struct inode *parentDirInode,
    const char *entryName, int mode, int umask, bool isExclusiveCreate,
    const struct FileEvent *fileEvent, CreateInfo *outCreateInfo)
#elif defined(KERNEL_HAS_USER_NS_MOUNTS)
void CreateInfo_init(App *app, struct user_namespace *mnt_userns, struct inode *parentDirInode,
    const char *entryName, int mode, int umask, bool isExclusiveCreate,
    const struct FileEvent *fileEvent, CreateInfo *outCreateInfo)
#else
void CreateInfo_init(App *app, struct inode *parentDirInode, const char *entryName,
    int mode, int umask, bool isExclusiveCreate, const struct FileEvent *fileEvent,
    CreateInfo *outCreateInfo)
#endif
{
   MountConfig *mountConfig = app->mountConfig;
   umode_t newMode = (umode_t)mode & ~(umode_t)umask;
   #ifdef LOG_DEBUG_MESSAGES
   Logger* log = App_getLogger(app);
   const char* logContext = "CreateInfo_init";
   #endif

#if defined(KERNEL_HAS_FS_ALLOW_IDMAP) && !defined(BEEGFS_DISABLE_IDMAPPING)
   uid_t creator_uid;
   gid_t creator_gid;

   #if defined(KERNEL_HAS_IDMAPPED_MOUNTS)
      kuid_t mfsuid;
      kgid_t mfsgid;
      struct user_namespace *fs_userns =
	      parentDirInode ? parentDirInode->i_sb->s_user_ns : &init_user_ns;
      if (idmap != &nop_mnt_idmap) {
         /* Get caller's fsuid mapped according to mount idmapping */
         mfsuid = mapped_fsuid(idmap, fs_userns);
         mfsgid = mapped_fsgid(idmap, fs_userns);
      } else {
         mfsuid = current_fsuid();
         mfsgid = current_fsgid();
      }
      /* from_kuid_munged : Create a uid from a kuid user-namespace pair */
      creator_uid = from_kuid_munged(fs_userns, mfsuid);
      creator_gid = from_kgid_munged(fs_userns, mfsgid);
      LOG_DEBUG_FORMATTED(log, Log_DEBUG, logContext,
         "[idmapped mount] Current process uid=%u gid=%u mapped to -> filesystem uid=%u gid=%u",
         FhgfsCommon_getCurrentUserID(), FhgfsCommon_getCurrentGroupID(), creator_uid, creator_gid);

   #elif defined(KERNEL_HAS_USER_NS_MOUNTS)
      /* 5.15–6.2: use mnt_userns for mapping */
      creator_uid = from_kuid_munged(mnt_userns, current_fsuid());
      creator_gid = from_kgid_munged(mnt_userns, current_fsgid());
      LOG_DEBUG_FORMATTED(log, Log_DEBUG, logContext,
         "[userns mount] Current process uid=%u gid=%u mapped to -> filesystem uid=%u gid=%u",
         FhgfsCommon_getCurrentUserID(), FhgfsCommon_getCurrentGroupID(), creator_uid, creator_gid);
   #else
      /* legacy */
      creator_uid = FhgfsCommon_getCurrentUserID();
      creator_gid = FhgfsCommon_getCurrentGroupID();
      LOG_DEBUG_FORMATTED(log, Log_DEBUG, logContext,
         "[legacy mount] uid=%u gid=%u",
         creator_uid, creator_gid);
   #endif

   outCreateInfo->userID = creator_uid;
   if (parentDirInode &&
      ((parentDirInode->i_mode & S_ISGID) || mountConfig->grpid)) {
      /* inherit parent gid (SGID or BeeGFS grpid behavior) */
      #if defined(KERNEL_HAS_IDMAPPED_MOUNTS)
	 /*
	  * parentDirInode->i_gid is already in the superblock’s userns.
	  * Only collapse to numeric form—don’t remap through idmap again.
	  */
	 gid_t parent_gid =
               from_kgid_munged(parentDirInode->i_sb->s_user_ns, parentDirInode->i_gid);
      #elif defined(KERNEL_HAS_USER_NS_MOUNTS)
         gid_t parent_gid = from_kgid_munged(mnt_userns, parentDirInode->i_gid);
      #else
         gid_t parent_gid = from_kgid(&init_user_ns, parentDirInode->i_gid);
      #endif

      outCreateInfo->groupID = parent_gid;

      /* only carry SGID bit for directories if parent had SGID set */
      if (S_ISDIR(newMode) && (parentDirInode->i_mode & S_ISGID))
         newMode |= S_ISGID;
   } else {
      /* otherwise use creator's fsgid */
      outCreateInfo->groupID = creator_gid;
   }
#else
   /*
    * No idmapped mounts: use process credentials directly
    * and keep the classic SGID/grpid inheritance semantics.
    */
   outCreateInfo->userID  = FhgfsCommon_getCurrentUserID();
   outCreateInfo->groupID = FhgfsCommon_getCurrentGroupID();
   LOG_DEBUG_FORMATTED(log, Log_DEBUG, logContext,
      "[id-mapping disabled] uid=%u gid=%u",
      outCreateInfo->userID, outCreateInfo->groupID);
   // groupID and mode logic similar to inode_init_owner()
   if (parentDirInode && ((parentDirInode->i_mode & S_ISGID) || mountConfig->grpid))
   {
      /* inherit parent gid */
      outCreateInfo->groupID = i_gid_read(parentDirInode);
      /* only carry SGID bit for directories if parent had SGID set */
      if (S_ISDIR(newMode) && (parentDirInode->i_mode & S_ISGID))
         newMode |= S_ISGID;
   }
#endif

   outCreateInfo->entryName = entryName;
   outCreateInfo->mode      = newMode;
   outCreateInfo->umask     = umask;
   outCreateInfo->isExclusiveCreate = isExclusiveCreate;

   outCreateInfo->preferredStorageTargets = App_getPreferredStorageTargets(app);
   outCreateInfo->preferredMetaTargets    = App_getPreferredMetaNodes(app);
   outCreateInfo->fileEvent = fileEvent;

   StoragePoolId_set(&(outCreateInfo->storagePoolId), STORAGEPOOLID_INVALIDPOOLID);
}


/**
 * @param outEntryInfo contained values will be kalloced (on success) and need to be kfreed with
 * FhgfsInode_freeEntryMinInfoVals() later.
 */
bool MetadataTk_getRootEntryInfoCopy(App* app, EntryInfo* outEntryInfo)
{
   NodeStoreEx* nodes = App_getMetaNodes(app);

   NodeOrGroup rootOwner = NodeStoreEx_getRootOwner(nodes);
   const char* parentEntryID = StringTk_strDup("");
   const char* entryID = StringTk_strDup(META_ROOTDIR_ID_STR);
   const char* dirName = StringTk_strDup("");
   DirEntryType entryType = (DirEntryType) DirEntryType_DIRECTORY;

   /* Even if rootOwner is invalid, we still init outEntryInfo and malloc as FhGFS
    * policy says that kfree(NULL) is not allowed (the kernel allows it). */

   EntryInfo_init(outEntryInfo, rootOwner, parentEntryID, entryID, dirName, entryType, 0);

   return NodeOrGroup_valid(rootOwner);
}
