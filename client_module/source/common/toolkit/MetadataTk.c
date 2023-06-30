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
 */
void CreateInfo_init(App* app, struct inode* parentDirInode, const char* entryName,
   int mode, int umask, bool isExclusiveCreate, const struct FileEvent* fileEvent,
   CreateInfo* outCreateInfo)
{
   MountConfig *mountConfig = app->mountConfig;

   outCreateInfo->userID = FhgfsCommon_getCurrentUserID();

   // groupID and mode logic taken from inode_init_owner()
   if (parentDirInode && ((parentDirInode->i_mode & S_ISGID) || mountConfig->grpid))
   {
      outCreateInfo->groupID = i_gid_read(parentDirInode);
      if (S_ISDIR(mode) && (parentDirInode->i_mode & S_ISGID))
         mode |= S_ISGID;
   }
   else
      outCreateInfo->groupID = FhgfsCommon_getCurrentGroupID();

   outCreateInfo->entryName = entryName;
   outCreateInfo->mode      = mode;
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
