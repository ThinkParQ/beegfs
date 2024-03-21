#include <app/App.h>
#include <app/config/Config.h>
#include <common/nodes/Node.h>
#include <common/nodes/MirrorBuddyGroupMapper.h>
#include <common/net/message/nodes/HeartbeatMsgEx.h>
#include <common/net/message/nodes/HeartbeatRequestMsgEx.h>
#include <common/toolkit/MathTk.h>
#include <filesystem/helper/IoctlHelper.h>
#include <filesystem/FhgfsOpsFile.h>
#include <os/OsCompat.h>
#include <linux/delay.h>
#include "FhgfsOpsSuper.h"
#include "FhgfsOpsHelper.h"
#include "FhgfsOpsIoctl.h"
#include "FhgfsOpsInode.h"

#ifdef CONFIG_COMPAT
#include <asm/compat.h>
#endif

#include <linux/mount.h>


/* old kernels didn't have the TCGETS ioctl definition (used by isatty() test) */
#ifndef TCGETS
#define TCGETS   0x5401
#endif

/* define these strings here so the IOCTL interface doesn't depend upon privately known ints */
#define FHGFSOPSIOCTL_NODETYPE_STORAGE    "storage"
#define FHGFSOPSIOCTL_NODETYPE_META       "meta"
#define FHGFSOPSIOCTL_NODETYPE_METADATA   "metadata"
#define FHGFSOPSIOCTL_NODETYPE_MGMT       "mgmt"
#define FHGFSOPSIOCTL_NODETYPE_MANAGEMENT "management"

static long FhgfsOpsIoctl_getCfgFile(struct file *file, void __user *argp);
static long FhgfsOpsIoctl_getRuntimeCfgFile(struct file *file, void __user *argp);
static long FhgfsOpsIoctl_testIsFhGFS(struct file *file, void __user *argp);
static long FhgfsOpsIoctl_createFile(struct file *file, void __user *argp, int version);
static long FhgfsOpsIoctl_getMountID(struct file *file, void __user *argp);
static long FhgfsOpsIoctl_getStripeInfo(struct file *file, void __user *argp);
static long FhgfsOpsIoctl_getStripeTarget(struct file *file, void __user *argp);
static long FhgfsOpsIoctl_getStripeTargetV2(struct file *file, void __user *argp);
static long FhgfsOpsIoctl_mkfileWithStripeHints(struct file *file, void __user *argp);
static long FhgfsOpsIoctl_getInodeID(struct file *file, void __user *argp);
static long FhgfsOpsIoctl_getEntryInfo(struct file *file, void __user *argp);
static long FhgfsOpsIoctl_pingNode(struct file *file, void __user *argp);

static NodeType FhgfsOpsIoctl_strToNodeType(const char* str, size_t len)
{
   if (!strncmp(FHGFSOPSIOCTL_NODETYPE_META, str, len) ||
      !strncmp(FHGFSOPSIOCTL_NODETYPE_METADATA, str, len))
      return NODETYPE_Meta;
   if (!strncmp(FHGFSOPSIOCTL_NODETYPE_STORAGE, str, len))
      return NODETYPE_Storage;
   if (!strncmp(FHGFSOPSIOCTL_NODETYPE_MGMT, str, len) ||
      !strncmp(FHGFSOPSIOCTL_NODETYPE_MANAGEMENT, str, len))
      return NODETYPE_Mgmt;
   return NODETYPE_Invalid;
}

/**
 * Execute FhGFS IOCTLs.
 *
 * @param file the file the ioctl was opened for
 * @param cmd the ioctl command supposed to be done
 * @param arg in and out argument
 */
long FhgfsOpsIoctl_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
   struct dentry *dentry = file_dentry(file);
   struct inode *inode = file_inode(file);
   App* app = FhgfsOps_getApp(dentry->d_sb);
   Logger* log = App_getLogger(app);
   const char* logContext = "FhgfsOps_ioctl";

   Logger_logFormatted(log, Log_SPAM, logContext, "Called ioctl cmd: %u", cmd);

   switch(cmd)
   {
      case BEEGFS_IOC_GETVERSION:
      case BEEGFS_IOC_GETVERSION_OLD:
      { /* used by userspace filesystems or userspace nfs servers for cases where a (recycled) inode
           number now refers to a different file (and thus has a different generation number) */
         return put_user(inode->i_generation, (int __user *) arg);
      } break;

      case BEEGFS_IOC_GET_CFG_FILE:
      { /* get the path to the client config file*/
         return FhgfsOpsIoctl_getCfgFile(file, (void __user *) arg);
      } break;

      case BEEGFS_IOC_CREATE_FILE:
      {
         return FhgfsOpsIoctl_createFile(file, (void __user *) arg, 1);
      } break;

      case BEEGFS_IOC_CREATE_FILE_V2:
      {
         return FhgfsOpsIoctl_createFile(file, (void __user *) arg, 2);
      } break;

      case BEEGFS_IOC_CREATE_FILE_V3:
      {
         return FhgfsOpsIoctl_createFile(file, (void __user *) arg, 3);
      } break;

      case BEEGFS_IOC_TEST_IS_FHGFS:
      {
         return FhgfsOpsIoctl_testIsFhGFS(file, (void __user *) arg);
      } break;

      case BEEGFS_IOC_GET_RUNTIME_CFG_FILE:
      { /* get the virtual runtime client config file (path to procfs) */
         return FhgfsOpsIoctl_getRuntimeCfgFile(file, (void __user *) arg);
      } break;

      case BEEGFS_IOC_GET_MOUNTID:
      { // get the mountID (e.g. for path in procfs)
         return FhgfsOpsIoctl_getMountID(file, (void __user *) arg);
      } break;

      case BEEGFS_IOC_GET_STRIPEINFO:
      { // get stripe info of a file
         return FhgfsOpsIoctl_getStripeInfo(file, (void __user *) arg);
      } break;

      case BEEGFS_IOC_GET_STRIPETARGET:
      { // get stripe info of a file
         return FhgfsOpsIoctl_getStripeTarget(file, (void __user *) arg);
      } break;

      case BEEGFS_IOC_GET_STRIPETARGET_V2:
         return FhgfsOpsIoctl_getStripeTargetV2(file, (void __user *) arg);

      case BEEGFS_IOC_MKFILE_STRIPEHINTS:
      { // create a file with stripe hints
         return FhgfsOpsIoctl_mkfileWithStripeHints(file, (void __user *) arg);
      }

      case BEEGFS_IOC_GETINODEID:
      { // calculate corresponding inodeID to given entryID
         return FhgfsOpsIoctl_getInodeID(file, (void __user *) arg);
      }

      case BEEGFS_IOC_GETENTRYINFO:
      { // return BeeGFS internal path to file
         return FhgfsOpsIoctl_getEntryInfo(file, (void __user *) arg);
      }

      case BEEGFS_IOC_PINGNODE:
      {
         return FhgfsOpsIoctl_pingNode(file, (void __user *) arg);
      }

      case TCGETS:
      { // filter isatty() test ioctl, which is often used by various standard tools
         return -ENOTTY;
      } break;

      default:
      {
         LOG_DEBUG_FORMATTED(log, Log_WARNING, logContext, "Unknown ioctl command code: %u", cmd);
         return -ENOIOCTLCMD;
      } break;
   }

   return 0;
}

#ifdef CONFIG_COMPAT
/**
 * Compatibility ioctl method for 64-bit kernels called from 32-bit user space
 */
long FhgfsOpsIoctl_compatIoctl(struct file *file, unsigned int cmd, unsigned long arg)
{
   struct dentry *dentry = file_dentry(file);
   App* app = FhgfsOps_getApp(dentry->d_sb);
   Logger* log = App_getLogger(app);
   const char* logContext = __func__;

   Logger_logFormatted(log, Log_SPAM, logContext, "Called ioctl cmd: %u", cmd);

   switch(cmd)
   {
      case BEEGFS_IOC32_GETVERSION_OLD:
      case BEEGFS_IOC32_GETVERSION:
      {
         cmd = BEEGFS_IOC_GETVERSION;
      } break;

      default:
      {
         return -ENOIOCTLCMD;
      }
   }

   return FhgfsOpsIoctl_ioctl(file, cmd, (unsigned long) compat_ptr(arg));
}
#endif // CONFIG_COMPAT

/**
 * Get the path to the client config file of this mount (e.g. /etc/fhgfs/fhgfs-client.conf).
 */
static long FhgfsOpsIoctl_getCfgFile(struct file *file, void __user *argp)
{
   struct dentry *dentry = file_dentry(file);
   App* app = FhgfsOps_getApp(dentry->d_sb);
   Logger* log = App_getLogger(app);
   const char* logContext = __func__;

   struct BeegfsIoctl_GetCfgFile_Arg __user *confFile = (struct BeegfsIoctl_GetCfgFile_Arg*)argp;
   Config* cfg = App_getConfig(app);
   char* fileName = Config_getCfgFile(cfg);
   int cpRes;

   int cfgFileStrLen = strlen(fileName) + 1; // strlen() does not count \0
   if (unlikely(cfgFileStrLen > BEEGFS_IOCTL_CFG_MAX_PATH))
   {
      Logger_logFormatted(log, Log_WARNING, logContext,
         "Config file path too long (%d vs max %d)", cfgFileStrLen, BEEGFS_IOCTL_CFG_MAX_PATH);
      return -EINVAL;
   }

   if(!os_access_ok(VERIFY_WRITE, confFile, sizeof(*confFile) ) )
   {
      Logger_logFormatted(log, Log_DEBUG, logContext, "access_ok() denied to write to conf_file");
      return -EINVAL;
   }

   cpRes = copy_to_user(&confFile->path[0], fileName, cfgFileStrLen); // also calls access_ok
   if(cpRes)
   {
      LOG_DEBUG_FORMATTED(log, Log_DEBUG, logContext, "copy_to_user failed()");
      return -EINVAL;
   }

   return 0;
}

/**
 * Get the path to the virtual runtime config file of this mount in procfs
 * (e.g. /proc/fs/beegfs/<mountID>/config).
 */
static long FhgfsOpsIoctl_getRuntimeCfgFile(struct file *file, void __user *argp)
{
   struct dentry *dentry = file_dentry(file);
   App* app = FhgfsOps_getApp(dentry->d_sb);
   Logger* log = App_getLogger(app);
   const char* logContext = __func__;

   struct BeegfsIoctl_GetCfgFile_Arg __user *confFile = (struct BeegfsIoctl_GetCfgFile_Arg*)argp;
   char* fileName = vmalloc(BEEGFS_IOCTL_CFG_MAX_PATH);

   Node* localNode = App_getLocalNode(app);
   char* localNodeID = Node_getID(localNode);

   int cpRes;
   int cfgFileStrLen;

   if(!os_access_ok(VERIFY_WRITE, confFile, sizeof(*confFile) ) )
   {
      Logger_logFormatted(log, Log_DEBUG, logContext, "access_ok() denied to write to conf_file");
      vfree(fileName);
      return -EINVAL;
   }

   cfgFileStrLen = scnprintf(fileName, BEEGFS_IOCTL_CFG_MAX_PATH, "/proc/fs/beegfs/%s/config",
      localNodeID);
   if (cfgFileStrLen <= 0)
   {
      Logger_logFormatted(log, Log_WARNING, logContext, "buffer too small");
      vfree(fileName);
      return -EINVAL;
   }

   cfgFileStrLen++; // scnprintf(...) does not count \0

   cpRes = copy_to_user(&confFile->path[0], fileName, cfgFileStrLen); // also calls access_ok
   if(cpRes)
   {
      LOG_DEBUG_FORMATTED(log, Log_DEBUG, logContext, "copy_to_user failed()");
      vfree(fileName);
      return -EINVAL;
   }

   vfree(fileName);
   return 0;
}

/**
 * Confirm to caller that we are a FhGFS mount.
 */
static long FhgfsOpsIoctl_testIsFhGFS(struct file *file, void __user *argp)
{
   struct dentry *dentry = file_dentry(file);
   App* app = FhgfsOps_getApp(dentry->d_sb);
   Logger* log = App_getLogger(app);
   const char* logContext = __func__;

   int cpRes = copy_to_user(argp,
      BEEGFS_IOCTL_TEST_STRING, sizeof(BEEGFS_IOCTL_TEST_STRING) ); // (also calls access_ok)
   if(cpRes)
   { // result >0 is number of uncopied bytes
      LOG_DEBUG_FORMATTED(log, Log_WARNING, logContext, "copy_to_user failed()");
      IGNORE_UNUSED_VARIABLE(log);
      IGNORE_UNUSED_VARIABLE(logContext);

      return -EINVAL;
   }

   return 0;
}

/**
 * Get the mountID aka clientID aka nodeID of client mount aka sessionID.
 */
static long FhgfsOpsIoctl_getMountID(struct file *file, void __user *argp)
{
   struct dentry *dentry = file_dentry(file);
   App* app = FhgfsOps_getApp(dentry->d_sb);
   Logger* log = App_getLogger(app);
   const char* logContext = __func__;

   Node* localNode = App_getLocalNode(app);
   const char* mountID = Node_getID(localNode);
   size_t mountIDStrLen = strlen(mountID);

   int cpRes;

   if(unlikely(mountIDStrLen > BEEGFS_IOCTL_MOUNTID_BUFLEN) )
   {
      Logger_logFormatted(log, Log_WARNING, logContext,
         "unexpected: mountID too large for buffer (%d vs %d)",
         (int)BEEGFS_IOCTL_MOUNTID_BUFLEN, (int)mountIDStrLen+1);

      return -ENOBUFS;
   }

   cpRes = copy_to_user(argp, mountID, mountIDStrLen+1); // (also calls access_ok)
   if(cpRes)
   { // result >0 is number of uncopied bytes
      LOG_DEBUG_FORMATTED(log, Log_WARNING, logContext, "copy_to_user failed()");

      return -EINVAL;
   }

   return 0;
}

/**
 * Get stripe info of a file (chunksize etc.).
 */
static long FhgfsOpsIoctl_getStripeInfo(struct file *file, void __user *argp)
{
   struct dentry* dentry = file_dentry(file);
   App* app = FhgfsOps_getApp(dentry->d_sb);
   Logger* log = App_getLogger(app);
   const char* logContext = __func__;

   struct inode* inode = file_inode(file);
   FhgfsInode* fhgfsInode = BEEGFS_INODE(inode);

   struct BeegfsIoctl_GetStripeInfo_Arg __user *getInfoArg =
      (struct BeegfsIoctl_GetStripeInfo_Arg*)argp;
   StripePattern* pattern = FhgfsInode_getStripePattern(fhgfsInode);

   uint16_t numTargets;
   int cpRes;

   if(S_ISDIR(inode->i_mode) )
   { // directories have no patterns attached
      return -EISDIR;
   }

   if(!pattern)
   { // sanity check, should never happen (because file pattern is set during file open)
      Logger_logFormatted(log, Log_DEBUG, logContext,
         "Given file handle has no stripe pattern attached");
      return -EINVAL;
   }


   // set pattern type

   cpRes = put_user(StripePattern_getPatternType(pattern), &getInfoArg->outPatternType);
   if(cpRes)
      return -EFAULT;

   // set chunksize

   cpRes = put_user(StripePattern_getChunkSize(pattern), &getInfoArg->outChunkSize);
   if(cpRes)
      return -EFAULT;

   // set number of stripe targets

   numTargets = UInt16Vec_length(pattern->getStripeTargetIDs(pattern) );

   cpRes = put_user(numTargets, &getInfoArg->outNumTargets);
   if(cpRes)
      return -EFAULT;

   return 0;
}

static long resolveNodeToString(NodeStoreEx* nodes, uint32_t nodeID,
   char __user* nodeStrID, Logger* log)
{
   NumNodeID numID = { nodeID };
   size_t nodeIDLen;
   long retVal = 0;

   Node* node = NodeStoreEx_referenceNode(nodes, numID);

   if (!node)
   {
      // node not found in store: set empty string as result
      if (copy_to_user(nodeStrID, "", 1))
      {
         LOG_DEBUG_FORMATTED(log, Log_DEBUG, __func__, "copy_to_user failed()");
         return -EINVAL;
      }

      return 0;
   }

   nodeIDLen = strlen(node->id) + 1;
   if (nodeIDLen > BEEGFS_IOCTL_NODESTRID_BUFLEN)
   { // nodeID too large for buffer
      retVal = -ENOBUFS;
      goto out;
   }

   if (copy_to_user(nodeStrID, node->id, nodeIDLen))
      retVal = -EFAULT;

out:
   Node_put(node);

   return retVal;
}

static long getStripePatternImpl(struct file* file, uint32_t targetIdx,
   uint32_t* targetOrGroup, uint32_t* primaryTarget, uint32_t* secondaryTarget,
   uint32_t* primaryNodeID, uint32_t* secondaryNodeID,
   char __user* primaryNodeStrID, char __user* secondaryNodeStrID)
{
   struct dentry* dentry = file_dentry(file);
   App* app = FhgfsOps_getApp(dentry->d_sb);
   Logger* log = App_getLogger(app);
   const char* logContext = __func__;
   TargetMapper* targetMapper = App_getTargetMapper(app);
   NodeStoreEx* storageNodes = App_getStorageNodes(app);

   struct inode* inode = dentry->d_inode;
   FhgfsInode* fhgfsInode = BEEGFS_INODE(inode);
   StripePattern* pattern = FhgfsInode_getStripePattern(fhgfsInode);

   long retVal = 0;

   size_t numTargets;

   // directories have no patterns attached
   if (S_ISDIR(inode->i_mode))
      return -EISDIR;

   if (!pattern)
   { // sanity check, should never happen (because file pattern is set during file open)
      Logger_logFormatted(log, Log_DEBUG, logContext,
         "Given file handle has no stripe pattern attached");
      return -EINVAL;
   }

   // check if wanted target index is valid
   numTargets = UInt16Vec_length(pattern->getStripeTargetIDs(pattern));
   if (targetIdx >= numTargets)
      return -EINVAL;

   // set targetID
   *targetOrGroup = UInt16Vec_at(pattern->getStripeTargetIDs(pattern), targetIdx);

   // resolve buddy group if necessary
   if (pattern->patternType == STRIPEPATTERN_BuddyMirror)
   {
      *primaryTarget = MirrorBuddyGroupMapper_getPrimaryTargetID(app->storageBuddyGroupMapper,
            *targetOrGroup);
      *secondaryTarget = MirrorBuddyGroupMapper_getSecondaryTargetID(app->storageBuddyGroupMapper,
            *targetOrGroup);
   }
   else
   {
      *primaryTarget = 0;
      *secondaryTarget = 0;
   }

   // resolve targets to nodes
   *primaryNodeID = TargetMapper_getNodeID(targetMapper,
         *primaryTarget ? *primaryTarget : *targetOrGroup).value;
   *secondaryNodeID = *secondaryTarget
      ? TargetMapper_getNodeID(targetMapper, *secondaryTarget).value
      : 0;

   // resolve node ID strings
   retVal = resolveNodeToString(storageNodes, *primaryNodeID, primaryNodeStrID, log);
   if (retVal)
      goto out;

   if (secondaryNodeStrID && *secondaryNodeID)
      retVal = resolveNodeToString(storageNodes, *secondaryNodeID, secondaryNodeStrID, log);

out:
   return retVal;
}

/**
 * Get stripe target of a file (index-based).
 */
static long FhgfsOpsIoctl_getStripeTarget(struct file *file, void __user *argp)
{
   struct BeegfsIoctl_GetStripeTarget_Arg __user* arg = argp;

   uint32_t targetOrGroup;
   uint32_t primaryTarget;
   uint32_t secondaryTarget;
   uint32_t primaryNodeID;
   uint32_t secondaryNodeID;

   uint16_t wantedTargetIndex;

   long retVal = 0;

   if (get_user(wantedTargetIndex, &arg->targetIndex))
      return -EFAULT;

   retVal = getStripePatternImpl(file, wantedTargetIndex, &targetOrGroup, &primaryTarget,
         &secondaryTarget, &primaryNodeID, &secondaryNodeID, arg->outNodeStrID, NULL);
   if (retVal)
      return retVal;

   if (put_user(targetOrGroup, &arg->outTargetNumID))
      return -EFAULT;

   if (put_user(primaryNodeID, &arg->outNodeNumID))
      return -EFAULT;

   return 0;
}

static long FhgfsOpsIoctl_getStripeTargetV2(struct file *file, void __user *argp)
{
   struct BeegfsIoctl_GetStripeTargetV2_Arg __user* arg = argp;

   uint32_t targetOrGroup;
   uint32_t primaryTarget;
   uint32_t secondaryTarget;
   uint32_t primaryNodeID;
   uint32_t secondaryNodeID;

   uint32_t wantedTargetIndex;

   long retVal = 0;

   if (get_user(wantedTargetIndex, &arg->targetIndex))
      return -EFAULT;

   retVal = getStripePatternImpl(file, wantedTargetIndex, &targetOrGroup, &primaryTarget,
         &secondaryTarget, &primaryNodeID, &secondaryNodeID, arg->primaryNodeStrID,
         arg->secondaryNodeStrID);
   if (retVal)
      return retVal;

   if (put_user(targetOrGroup, &arg->targetOrGroup))
      return -EFAULT;

   if (put_user(primaryTarget, &arg->primaryTarget))
      return -EFAULT;

   if (put_user(secondaryTarget, &arg->secondaryTarget))
      return -EFAULT;

   if (put_user(primaryNodeID, &arg->primaryNodeID))
      return -EFAULT;

   if (put_user(secondaryNodeID, &arg->secondaryNodeID))
      return -EFAULT;

   return 0;
}

/**
 * Create a new regular file with stripe hints (chunksize, numtargets).
 *
 * @param file parent directory of the new file
 */
long FhgfsOpsIoctl_mkfileWithStripeHints(struct file *file, void __user *argp)
{
   struct dentry* dentry = file_dentry(file);
   struct inode* parentInode = file_inode(file);
   FhgfsInode* fhgfsParentInode = BEEGFS_INODE(parentInode);

   App* app = FhgfsOps_getApp(dentry->d_sb);
   Logger* log = App_getLogger(app);
   const char* logContext = __func__;
   const int umask = current_umask();

   struct BeegfsIoctl_MkFileWithStripeHints_Arg __user *mkfileArg =
      (struct BeegfsIoctl_MkFileWithStripeHints_Arg*)argp;

   long retVal;

   const char __user* userFilename;
   char* filename;
   int mode; // mode (permission) of the new file
   unsigned numtargets; // number of desired targets, 0 for directory default
   unsigned chunksize; // must be 64K or multiple of 64K, 0 for directory default

   int cpRes;
   const EntryInfo* parentEntryInfo;
   FhgfsOpsErr mkRes;

   struct FileEvent event = FILEEVENT_EMPTY;
   struct FileEvent* eventSent = NULL;

   struct CreateInfo createInfo =
   {
      .preferredStorageTargets = NULL,
      .preferredMetaTargets = NULL
   };

#ifdef KERNEL_HAS_FILE_F_VFSMNT
   struct vfsmount* mnt = file->f_vfsmnt;
#else
   struct vfsmount* mnt = file->f_path.mnt;
#endif

   Logger_logFormatted(log, Log_SPAM, logContext, "Create file from ioctl");

   if(!S_ISDIR(parentInode->i_mode) )
   { // given inode does not refer to a directory
      return -ENOTDIR;
   }

   retVal = os_generic_permission(parentInode, MAY_WRITE | MAY_EXEC);
   if (retVal)
      return retVal;

   // copy mode

   cpRes = get_user(mode, &mkfileArg->mode);
   if(cpRes)
      return -EFAULT;

   // make sure we only use permissions bits of given mode and set regular file as format bit
   mode = (mode & (S_IRWXU | S_IRWXG | S_IRWXO) ) | S_IFREG;

   // copy numtargets

   cpRes = get_user(numtargets, &mkfileArg->numtargets);
   if(cpRes)
      return -EFAULT;

   // copy chunksize

   cpRes = get_user(chunksize, &mkfileArg->chunksize);
   if(cpRes)
      return -EFAULT;

   if (get_user(userFilename, &mkfileArg->filename))
      return -EFAULT;

   if (chunksize != 0)
   { // check if chunksize is valid
      if(unlikely( (chunksize < STRIPEPATTERN_MIN_CHUNKSIZE) ||
                   !MathTk_isPowerOfTwo(chunksize) ) )
         return -EINVAL; // chunksize is not a multiple of 64Ki
   }


   // check and reference mnt write counter

   retVal = mnt_want_write(mnt);
   if(retVal)
      return retVal;

   // copy filename

   filename = strndup_user(userFilename, BEEGFS_IOCTL_FILENAME_MAXLEN);
   if(IS_ERR(filename) )
   {
      retVal = PTR_ERR(filename);
      goto err_cleanup_lock;
   }

   if (app->cfg->eventLogMask & EventLogMask_LINK_OP)
   {
      // we don't have a dentry for the target file here. we could use the dentry to the directory
      // and the requested file name, but decided to not bother yet because ioctl operations are
      // expected to be so rare.
      FileEvent_init(&event, FileEventType_CREATE, NULL);
      eventSent = &event;
   }

   CreateInfo_init(app, parentInode, filename, mode, umask, true, eventSent, &createInfo);

   FhgfsInode_entryInfoReadLock(fhgfsParentInode); // L O C K EntryInfo

   parentEntryInfo = FhgfsInode_getEntryInfo(fhgfsParentInode);

   mkRes = FhgfsOpsRemoting_mkfileWithStripeHints(app, parentEntryInfo, &createInfo,
      numtargets, chunksize, NULL);

   FhgfsInode_entryInfoReadUnlock(fhgfsParentInode); // U N L O C K EntryInfo

   if(mkRes != FhgfsOpsErr_SUCCESS)
   {
      retVal = FhgfsOpsErr_toSysErr(mkRes); // (note: newEntryInfo was not alloc'ed)
      goto err_cleanup_filename;
   }


   retVal = 0;

err_cleanup_filename:
   // cleanup
   kfree(filename);

   FileEvent_uninit(&event);

err_cleanup_lock:

   mnt_drop_write(mnt); // release mnt write reference counter

   return retVal;
}

/**
 * create a file with special settings (such as preferred targets).
 *
 * @return 0 on success, negative linux error code otherwise
 */
static long FhgfsOpsIoctl_createFile(struct file *file, void __user *argp, int version)
{
   struct dentry *dentry = file_dentry(file);
   struct inode *inode = file_inode(file);

   App* app = FhgfsOps_getApp(dentry->d_sb);
   Logger* log = App_getLogger(app);
   const char* logContext = __func__;
   const int umask = current_umask();
   StoragePoolId storagePoolId;

   EntryInfo parentInfo;
   struct BeegfsIoctl_MkFileV3_Arg fileInfo;
   struct CreateInfo createInfo =
   {
      .preferredStorageTargets = NULL,
      .preferredMetaTargets = NULL
   };

   int retVal = 0;
   FhgfsOpsErr mkRes;

   struct FileEvent event = FILEEVENT_EMPTY;
   const struct FileEvent* eventSent = NULL;

   #ifdef KERNEL_HAS_FILE_F_VFSMNT
      struct vfsmount* mnt = file->f_vfsmnt;
   #else
      struct vfsmount* mnt = file->f_path.mnt;
   #endif


   Logger_logFormatted(log, Log_SPAM, logContext, "Create file from ioctl");

   retVal = os_generic_permission(inode, MAY_WRITE | MAY_EXEC);
   if (retVal)
      return retVal;

   retVal = mnt_want_write(mnt); // check and rw-reference counter
   if (retVal)
      return retVal;


   memset(&fileInfo, 0, sizeof(struct BeegfsIoctl_MkFileV3_Arg));

   switch(version)
   {
      case 1: retVal = IoctlHelper_ioctlCreateFileCopyFromUser(app, argp, &fileInfo); break;
      case 2: retVal = IoctlHelper_ioctlCreateFileCopyFromUserV2(app, argp, &fileInfo); break;
      case 3: retVal = IoctlHelper_ioctlCreateFileCopyFromUserV3(app, argp, &fileInfo); break;
      default: retVal = IoctlHelper_ioctlCreateFileCopyFromUser(app, argp, &fileInfo); break;
   }

   if(retVal)
   {
      LOG_DEBUG_FORMATTED(log, Log_SPAM, logContext,
         "Copy from user of struct Fhgfs_ioctlMkFile failed");
      goto cleanup;
   }

   // the actual event is initialized appropriatly later
   if (app->cfg->eventLogMask & EventLogMask_LINK_OP)
      eventSent = &event;

   CreateInfo_init(app, inode, fileInfo.entryName, fileInfo.mode, umask, true, eventSent,
      &createInfo);

   StoragePoolId_set(&storagePoolId, fileInfo.storagePoolId);
   CreateInfo_setStoragePoolId(&createInfo, storagePoolId);

   retVal = IoctlHelper_ioctlCreateFileTargetsToList(app, &fileInfo, &createInfo); // target list
   if (retVal)
      goto cleanup;

   // only use the provided UID and GID if we are root
   /* note: this means we create the file with different uid/gid without notifying the caller. maybe
      we should change that. */
   if (FhgfsCommon_getCurrentUserID() == 0 || FhgfsCommon_getCurrentGroupID() == 0)
   {
      createInfo.userID  = fileInfo.uid;
      createInfo.groupID = fileInfo.gid;
   }

   if (fileInfo.parentIsBuddyMirrored)
      EntryInfo_init(&parentInfo, NodeOrGroup_fromGroup(fileInfo.ownerNodeID),
            fileInfo.parentParentEntryID, fileInfo.parentEntryID, fileInfo.parentName,
            DirEntryType_DIRECTORY, STATFLAG_HINT_INLINE);
   else
   {
      NumNodeID nodeID;
      NumNodeID_set(&nodeID, fileInfo.ownerNodeID);
      EntryInfo_init(&parentInfo, NodeOrGroup_fromNode(nodeID),
            fileInfo.parentParentEntryID, fileInfo.parentEntryID, fileInfo.parentName,
            DirEntryType_DIRECTORY, STATFLAG_HINT_INLINE);
   }

   switch (fileInfo.fileType)
   {
      case DT_REG:
      {
         // we don't have a dentry for the target file here. we could use the dentry to the directory
         // and the requested file name, but decided to not bother yet because ioctl operations are
         // expected to be so rare.
         if (app->cfg->eventLogMask & EventLogMask_LINK_OP)
            FileEvent_init(&event, FileEventType_CREATE, NULL);

         down_read(&inode->i_sb->s_umount);

         LOG_DEBUG_FORMATTED(log, Log_SPAM, logContext, "Going to create file %s",
            createInfo.entryName);

         mkRes = FhgfsOpsRemoting_mkfile(app, &parentInfo, &createInfo, NULL);
         if(mkRes != FhgfsOpsErr_SUCCESS)
         {  // failure (note: no need to care about newEntryInfo, it was not allocated)
            LOG_DEBUG_FORMATTED(log, Log_SPAM, logContext, "mkfile failed");

            retVal = FhgfsOpsErr_toSysErr(mkRes);
         }

         up_read(&inode->i_sb->s_umount);

         FileEvent_uninit(&event);
      } break;

      case DT_LNK:
      {
         EntryInfo newEntryInfo; // only needed as mandatory argument

         if (!fileInfo.symlinkTo)
         {
            retVal = -EINVAL;
            goto cleanup;
         }

         if (app->cfg->eventLogMask & EventLogMask_LINK_OP)
         {
            event.eventType = FileEventType_SYMLINK;
            FileEvent_setTargetStr(&event, fileInfo.symlinkTo);
         }

         down_read(&inode->i_sb->s_umount);

         // destroys &event
         mkRes = FhgfsOpsHelper_symlink(app, &parentInfo, fileInfo.symlinkTo,
            &createInfo, &newEntryInfo);

         if(mkRes != FhgfsOpsErr_SUCCESS)
         {  // failed (note: no need to care about newEntryInfo, it was not allocated)
            LOG_DEBUG_FORMATTED(log, Log_SPAM, logContext, "mkfile failed");

            retVal = FhgfsOpsErr_toSysErr(mkRes);
         }
         else
            EntryInfo_uninit(&newEntryInfo); // unit newEntryInfo, we don't need it

         up_read(&inode->i_sb->s_umount);
      } break;

      default:
      {
         // unsupported file type
         LOG_DEBUG_FORMATTED(log, Log_NOTICE, logContext, "Unsupported file type: %d",
            fileInfo.fileType);
         retVal = -EINVAL;
         goto cleanup;
      } break;
   }

cleanup:
   SAFE_KFREE(fileInfo.parentParentEntryID);
   SAFE_KFREE(fileInfo.parentEntryID);
   SAFE_KFREE(fileInfo.parentName);
   SAFE_KFREE(fileInfo.entryName);
   SAFE_KFREE(fileInfo.symlinkTo);
   SAFE_KFREE(fileInfo.prefTargets);
   if (createInfo.preferredStorageTargets &&
         createInfo.preferredStorageTargets != App_getPreferredStorageTargets(app) )
   {
      UInt16List_uninit(createInfo.preferredStorageTargets);
      SAFE_KFREE(createInfo.preferredStorageTargets);
   }
   if (createInfo.preferredMetaTargets &&
         createInfo.preferredMetaTargets != App_getPreferredMetaNodes(app) )
   {
      UInt16List_uninit(createInfo.preferredMetaTargets);
      SAFE_KFREE(createInfo.preferredMetaTargets);
   }

   mnt_drop_write(mnt); // release the rw-reference counter

   return retVal;
}

static long FhgfsOpsIoctl_getInodeID(struct file *file, void __user *argp)
{
   struct BeegfsIoctl_GetInodeID_Arg __user *getInodeIDArg = argp;

   struct super_block* superBlock = file_dentry(file)->d_sb;
   Logger* log = App_getLogger(FhgfsOps_getApp(superBlock));
   uint64_t inodeID;

   char entryID[BEEGFS_IOCTL_ENTRYID_MAXLEN + 1];

   if (copy_from_user(entryID, &getInodeIDArg->entryID, sizeof(entryID)))
   {
      Logger_logFormatted(log, Log_DEBUG, __func__,
            "Copying entryID from userspace memory failed.");
      return -EFAULT;
   }

   inodeID = FhgfsInode_generateInodeID(superBlock,
        entryID, strnlen(entryID, sizeof(entryID)));

   if (put_user(inodeID, &getInodeIDArg->inodeID))
   {
      Logger_logFormatted(log, Log_DEBUG, __func__,
            "Copying inodeID to userspace memory failed.");
      return -EFAULT;
   }

   return 0;
}

static long FhgfsOpsIoctl_pingNode(struct file *file, void __user *argp)
{
   struct super_block* superBlock = file_dentry(file)->d_sb;
   App* app = FhgfsOps_getApp(superBlock);
   Logger* log = App_getLogger(app);
   NoAllocBufferStore* bufStore = App_getMsgBufStore(app);
   NodeStoreEx* nodeStore;
   Node* node;
   NodeConnPool* connPool;
   Socket* sock = NULL;
   struct BeegfsIoctl_PingNode_Arg __user *pingArg = argp;
   struct BeegfsIoctl_PingNode_Arg ping;
   NumNodeID nodeId;
   long rc;
   int i;
   NodeType nodeType;

   if (copy_from_user(&ping.params, &pingArg->params, sizeof(ping.params)))
   {
      Logger_logFormatted(log, Log_ERR, __func__,
            "Copying ping arguments from userspace memory failed.");
      return -EFAULT;
   }

   ping.params.nodeType[BEEGFS_IOCTL_NODETYPE_BUFLEN - 1] = 0;
   Logger_logFormatted(log, Log_DEBUG, __func__, "nodeId: %u nodeType: %s count=%u",
      ping.params.nodeId, ping.params.nodeType, ping.params.count);

   memset(&ping.results, 0, sizeof(ping.results));

   if (ping.params.count > BEEGFS_IOCTL_PING_MAX_COUNT)
   {
      Logger_logFormatted(log, Log_ERR, __func__,
         "count too high, max is %d", BEEGFS_IOCTL_PING_MAX_COUNT);
      return -EINVAL;
   }

   if (ping.params.interval > BEEGFS_IOCTL_PING_MAX_INTERVAL)
   {
      Logger_logFormatted(log, Log_ERR, __func__,
         "interval too high, max is %d", BEEGFS_IOCTL_PING_MAX_INTERVAL);
      return -EINVAL;
   }

   NumNodeID_set(&nodeId, ping.params.nodeId);
   nodeType = FhgfsOpsIoctl_strToNodeType(ping.params.nodeType, sizeof(ping.params.nodeType));
   switch (nodeType)
   {
   case NODETYPE_Meta:
      nodeStore = App_getMetaNodes(app);
      break;
   case NODETYPE_Storage:
      nodeStore = App_getStorageNodes(app);
      break;
   case NODETYPE_Mgmt:
      nodeStore = App_getMgmtNodes(app);
      break;
   default:
      Logger_logFormatted(log, Log_ERR, __func__,
         "Invalid nodetype: %s", ping.params.nodeType);
      return -EINVAL;
   }

   node = NodeStoreEx_referenceNode(nodeStore, nodeId);
   if (node == NULL)
   {
      Logger_logFormatted(log, Log_ERR, __func__,
         "Node not found. nodeId: %u nodeType: %s", ping.params.nodeId, ping.params.nodeType);
      return -EINVAL;
   }

   StringTk_strncpyTerminated(ping.results.outNode, Node_getID(node),
      sizeof(ping.results.outNode));

   connPool = Node_getConnPool(node);

   rc = 0;

   for (i = 0; i <= ping.params.count; ++i)
   {
      HeartbeatMsgEx* rspMsgEx;
      char* respBuf = NULL;
      NetMessage* respMsg = NULL;
      HeartbeatRequestMsgEx msg;
      FhgfsOpsErr requestRes;
      Time startTime;
      Time endTime;
      NumNodeID resNodeID;
      int resNodeType;

      if (i > 0)
         msleep(ping.params.interval);

      if (sock == NULL)
      {
         sock = NodeConnPool_acquireStreamSocket(connPool);
         if (sock == NULL)
         {
            Logger_logFormatted(log, Log_ERR, __func__,
               "Null socket from connPool");
            rc = -EINVAL;
            goto next_iter;
         }

      }

      HeartbeatRequestMsgEx_init(&msg);
      Time_init(&startTime);
      requestRes = MessagingTk_requestResponseSock(app, node, (NetMessage*)&msg, NETMSGTYPE_Heartbeat, &respBuf, &respMsg, sock);
      Time_init(&endTime);

      if (requestRes != FhgfsOpsErr_SUCCESS)
      {
         NodeConnPool_invalidateStreamSocket(connPool, sock);
         sock = NULL;
         ping.results.outErrors++;
         goto next_iter;
      }

      // Skip the first one because it may have connection overhead.
      if (i == 0)
         goto next_iter;


      rspMsgEx = (HeartbeatMsgEx*) respMsg;
      resNodeID = HeartbeatMsgEx_getNodeNumID(rspMsgEx);
      resNodeType = HeartbeatMsgEx_getNodeType(rspMsgEx);

      if (!NumNodeID_compare(&resNodeID, &nodeId))
      {
         Logger_logFormatted(log, Log_ERR, __func__,
            "Wrong node ID returned from heartbeat: %s",  NumNodeID_str(&resNodeID));
         rc = -EPROTO;
      }
      else if (resNodeType != (int) nodeType)
      {
         Logger_logFormatted(log, Log_ERR, __func__,
            "Wrong node type returned from heartbeat: %d", resNodeType);
         rc = -EPROTO;
      }
      else
      {
         unsigned elapsed = Time_elapsedSinceNS(&endTime, &startTime);
         ping.results.outSuccess++;
         ping.results.outPingTime[i - 1] = elapsed;
         ping.results.outTotalTime += elapsed;
         StringTk_strncpyTerminated(ping.results.outPingType[i - 1],
            NIC_nicTypeToString(Socket_getSockType(sock)),
            sizeof(ping.results.outPingType[i - 1]));
      }

   next_iter:
      if (respMsg)
         NETMESSAGE_FREE(respMsg);
      if (respBuf)
         NoAllocBufferStore_addBuf(bufStore, respBuf);

      if (rc != 0)
         break;
   }
   if (sock != NULL)
      NodeConnPool_releaseStreamSocket(connPool, sock);
   Node_put(node);
   if (copy_to_user(&pingArg->results, &ping.results, sizeof(ping.results)))
   {
      Logger_logFormatted(log, Log_ERR, __func__,
            "Copying ping results to userspace memory failed.");
      rc = -EFAULT;
   }

   return rc;
}

/**
 * Get EntryInfo data for given file.
 *
 * @return 0 on success, negative linux error code otherwise
 */
static long FhgfsOpsIoctl_getEntryInfo(struct file *file, void __user *argp)
{
   struct BeegfsIoctl_GetEntryInfo_Arg __user *arg = argp;
   struct dentry* fileDentry = file_dentry(file);

   App* app = FhgfsOps_getApp(fileDentry->d_sb);
   Logger* log = App_getLogger(app);

   FhgfsInode* beegfsInode = BEEGFS_INODE(fileDentry->d_inode);

   const EntryInfo* entryInfo;

   FhgfsInode_entryInfoReadLock(beegfsInode);

   entryInfo = FhgfsInode_getEntryInfo(beegfsInode);


   if (put_user(EntryInfo_getOwner(entryInfo), &arg->ownerID))
      goto fail;

   if (copy_to_user(arg->parentEntryID, EntryInfo_getParentEntryID(entryInfo),
         strnlen(entryInfo->parentEntryID, BEEGFS_IOCTL_ENTRYID_MAXLEN) + 1))
      goto fail;

   if (copy_to_user(arg->entryID, EntryInfo_getEntryID(entryInfo),
         strnlen(entryInfo->entryID, BEEGFS_IOCTL_ENTRYID_MAXLEN) + 1))
      goto fail;

   if (put_user(entryInfo->entryType, &arg->entryType))
      goto fail;

   if (put_user(entryInfo->featureFlags, &arg->featureFlags))
      goto fail;

   FhgfsInode_entryInfoReadUnlock(beegfsInode);
   return 0;

fail:
   FhgfsInode_entryInfoReadUnlock(beegfsInode);
   Logger_logFormatted(log, Log_DEBUG, __func__, "Copying entryInfo to userspace memory failed.");
   return -EFAULT;
}
