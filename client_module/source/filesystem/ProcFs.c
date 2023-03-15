#include <filesystem/ProcFsHelper.h>
#include "ProcFs.h"

#include <linux/fs.h>
#include <linux/stat.h>


#define BEEGFS_PROC_DIR_NAME      "fs/" BEEGFS_MODULE_NAME_STR
#define BEEGFS_PROC_NAMEBUF_LEN   4096

#define BEEGFS_PROC_ENTRY_CONFIG              "config"
#define BEEGFS_PROC_ENTRY_STATUS              ".status"
#define BEEGFS_PROC_ENTRY_MGMTNODES           "mgmt_nodes"
#define BEEGFS_PROC_ENTRY_METANODES           "meta_nodes"
#define BEEGFS_PROC_ENTRY_STORAGENODES        "storage_nodes"
#define BEEGFS_PROC_ENTRY_CLIENTINFO          "client_info"
#define BEEGFS_PROC_ENTRY_RETRIESENABLED      "conn_retries_enabled"
#define BEEGFS_PROC_ENTRY_NETBENCHENABLED     "netbench_mode"
#define BEEGFS_PROC_ENTRY_DROPCONNS           "drop_conns"
#define BEEGFS_PROC_ENTRY_LOGLEVELS           "log_levels"
#define BEEGFS_PROC_ENTRY_METATARGETSTATES    "meta_target_state"
#define BEEGFS_PROC_ENTRY_STORAGETARGETSTATES "storage_target_state"
#define BEEGFS_PROC_ENTRY_REMAPCONNFAILURE    "remap_connection_failure"


/**
 * Initializer for read-only proc file ops
 */
#if defined(KERNEL_HAS_PROC_OPS)
#define BEEGFS_PROC_FOPS_INITIALIZER  \
   .proc_open    = __ProcFs_open,     \
   .proc_read    = seq_read,          \
   .proc_lseek  = seq_lseek,          \
   .proc_release = single_release
#else
#define BEEGFS_PROC_FOPS_INITIALIZER  \
   .open    = __ProcFs_open,         \
   .read    = seq_read,              \
   .llseek  = seq_lseek,             \
   .release = single_release
#endif

#if defined(KERNEL_HAS_PROC_OPS)
#define PROC_OPS_WRITE_MEMBER proc_write
#else
#define PROC_OPS_WRITE_MEMBER write
#endif


/**
 * generic file ops for procfs entries
 */
#if defined(KERNEL_HAS_PROC_OPS)
static const struct proc_ops fhgfs_proc_fops =
#else
static const struct file_operations fhgfs_proc_fops =
#endif
{
   BEEGFS_PROC_FOPS_INITIALIZER
};

/*
 * for read-only entries: contains name and "show method" for a procfs entry
 */
struct fhgfs_proc_file
{
   char name[32]; // filename
   int (*show)(struct seq_file *, void *); // the "show method" of this file
};

/**
 * all our read-only procfs entries (terminated by empty element)
 */
static const struct fhgfs_proc_file fhgfs_proc_files[] =
{
   { BEEGFS_PROC_ENTRY_CONFIG, &__ProcFs_readV2_config },
   { BEEGFS_PROC_ENTRY_STATUS, &__ProcFs_readV2_status },
   { BEEGFS_PROC_ENTRY_MGMTNODES, &__ProcFs_readV2_mgmtNodes },
   { BEEGFS_PROC_ENTRY_METANODES, &__ProcFs_readV2_metaNodes },
   { BEEGFS_PROC_ENTRY_STORAGENODES, &__ProcFs_readV2_storageNodes },
   { BEEGFS_PROC_ENTRY_CLIENTINFO, &__ProcFs_readV2_clientInfo },
   { BEEGFS_PROC_ENTRY_METATARGETSTATES, &__ProcFs_readV2_metaTargetStates },
   { BEEGFS_PROC_ENTRY_STORAGETARGETSTATES, &__ProcFs_readV2_storageTargetStates },
   { "", NULL } // last element must be empty (for loop termination)
};

/*
 * for read+write entries: contains name, show method and write method for a procfs entry
 */
struct fhgfs_proc_file_rw
{
   char name[32]; // filename
   int (*show)(struct seq_file *, void *); // the show method of this file
   //ssize_t (*write)(struct file *, const char __user *, size_t, loff_t *); // the write method
#if defined(KERNEL_HAS_PROC_OPS)
   struct proc_ops proc_fops;
#else
   struct file_operations proc_fops;
#endif
};


/**
 * all our read+write procfs entries (terminated by empty element).
 *
 * other than for read-only entries, we need to assign different write methods during proc entry
 * registration, so we need indidivual file_operations for each entry here.
 */
static const struct fhgfs_proc_file_rw fhgfs_proc_files_rw[] =
{
   { BEEGFS_PROC_ENTRY_RETRIESENABLED, &__ProcFs_readV2_connRetriesEnabled,
      {
         BEEGFS_PROC_FOPS_INITIALIZER,
         .PROC_OPS_WRITE_MEMBER   = &__ProcFs_writeV2_connRetriesEnabled,
      },
   },
   { BEEGFS_PROC_ENTRY_NETBENCHENABLED, &__ProcFs_readV2_netBenchModeEnabled,
      {
         BEEGFS_PROC_FOPS_INITIALIZER,
         .PROC_OPS_WRITE_MEMBER   = &__ProcFs_writeV2_netBenchModeEnabled,
      },
   },
   { BEEGFS_PROC_ENTRY_DROPCONNS, &__ProcFs_readV2_nothing,
      {
         BEEGFS_PROC_FOPS_INITIALIZER,
         .PROC_OPS_WRITE_MEMBER   = &__ProcFs_writeV2_dropConns,
      },
   },
   { BEEGFS_PROC_ENTRY_LOGLEVELS, &__ProcFs_readV2_logLevels,
      {
         BEEGFS_PROC_FOPS_INITIALIZER,
         .PROC_OPS_WRITE_MEMBER = &__ProcFs_writeV2_logLevels,
      },
   },
   { BEEGFS_PROC_ENTRY_REMAPCONNFAILURE, &__ProcFs_read_remapConnectionFailure,
      {
         BEEGFS_PROC_FOPS_INITIALIZER,
         .PROC_OPS_WRITE_MEMBER   = &__ProcFs_write_remapConnectionFailure,
      },
   },
   // last element must be empty (for loop termination)
   {{ 0 }}
};


/**
 * Creates the general parent dir. Meant to be called only once at module load.
 */
void ProcFs_createGeneralDir(void)
{
   struct proc_dir_entry* procDir;

   procDir = proc_mkdir(BEEGFS_PROC_DIR_NAME, NULL);
   if(!procDir)
      printk_fhgfs(KERN_INFO, "Failed to create proc dir: " BEEGFS_PROC_DIR_NAME "\n");
}

/**
 * Removes the general parent dir. Meant to be called only once at module unload.
 */
void ProcFs_removeGeneralDir(void)
{
   remove_proc_entry(BEEGFS_PROC_DIR_NAME, NULL);
}

/**
 * Creates the dir and entries for a specific mountpoint.
 * Note: Uses sessionID to create a unique dir for the mountpoint.
 */
void ProcFs_createEntries(App* app)
{
   const char* sessionID = ProcFsHelper_getSessionID(app);

   struct proc_dir_entry* procDir;

   const struct fhgfs_proc_file* procFile;
   const struct fhgfs_proc_file_rw* procFileRW;


   // create unique directory for this clientID and store app pointer as "->data"

   char* dirNameBuf = vmalloc(BEEGFS_PROC_NAMEBUF_LEN);

   scnprintf(dirNameBuf, BEEGFS_PROC_NAMEBUF_LEN, BEEGFS_PROC_DIR_NAME "/%s", sessionID);

   procDir = __ProcFs_mkDir(dirNameBuf, app);
   if(!procDir)
   {
      printk_fhgfs(KERN_INFO, "Failed to create proc dir: %s\n", dirNameBuf);
      goto clean_up;
   }


   // create entries


   /* note: linux-3.10 kills create_proc_(read_)entry and uses proc_create_data instead.
      proc_create_data existed for ealier linux version already, so we use it there, too. */


   // create read-only proc files

   for(procFile = fhgfs_proc_files; procFile->name[0]; procFile++)
   {
      struct proc_dir_entry* currentProcFsEntry = proc_create_data(
         procFile->name, S_IFREG | S_IRUGO, procDir, &fhgfs_proc_fops, procFile->show);

      if(!currentProcFsEntry)
      {
         printk_fhgfs(KERN_INFO, "Failed to create read-only proc entry in %s: %s\n",
            dirNameBuf, procFile->name);
         goto clean_up;
      }
   }

   // create read+write proc files

   for(procFileRW = fhgfs_proc_files_rw; procFileRW->name[0]; procFileRW++)
   {
      struct proc_dir_entry* currentProcFsEntry = proc_create_data(
         procFileRW->name, S_IFREG | S_IRUGO | S_IWUSR | S_IWGRP, procDir, &procFileRW->proc_fops,
         procFileRW->show);

      if(!currentProcFsEntry)
      {
         printk_fhgfs(KERN_INFO, "Failed to create read+write proc entry in %s: %s\n",
            dirNameBuf, procFileRW->name);
         goto clean_up;
      }
   }

clean_up:
   SAFE_VFREE(dirNameBuf);
}

/**
 * Removes the dir and entries for a specific mountpoint.
 * Note: Uses sessionID for unique dir of the mountpoint.
 */
void ProcFs_removeEntries(App* app)
{
   char* dirNameBuf = vmalloc(BEEGFS_PROC_NAMEBUF_LEN);
   char* entryNameBuf = vmalloc(BEEGFS_PROC_NAMEBUF_LEN);

   const char* sessionID = ProcFsHelper_getSessionID(app);

   scnprintf(dirNameBuf, BEEGFS_PROC_NAMEBUF_LEN, BEEGFS_PROC_DIR_NAME "/%s", sessionID);

   // remove entries
   scnprintf(entryNameBuf, BEEGFS_PROC_NAMEBUF_LEN, "%s/%s",
      dirNameBuf, BEEGFS_PROC_ENTRY_CONFIG);
   remove_proc_entry(entryNameBuf, NULL);

   scnprintf(entryNameBuf, BEEGFS_PROC_NAMEBUF_LEN, "%s/%s",
      dirNameBuf, BEEGFS_PROC_ENTRY_STATUS);
   remove_proc_entry(entryNameBuf, NULL);

   scnprintf(entryNameBuf, BEEGFS_PROC_NAMEBUF_LEN, "%s/%s",
      dirNameBuf, BEEGFS_PROC_ENTRY_MGMTNODES);
   remove_proc_entry(entryNameBuf, NULL);

   scnprintf(entryNameBuf, BEEGFS_PROC_NAMEBUF_LEN, "%s/%s",
      dirNameBuf, BEEGFS_PROC_ENTRY_METANODES);
   remove_proc_entry(entryNameBuf, NULL);

   scnprintf(entryNameBuf, BEEGFS_PROC_NAMEBUF_LEN, "%s/%s",
      dirNameBuf, BEEGFS_PROC_ENTRY_STORAGENODES);
   remove_proc_entry(entryNameBuf, NULL);

   scnprintf(entryNameBuf, BEEGFS_PROC_NAMEBUF_LEN, "%s/%s",
      dirNameBuf, BEEGFS_PROC_ENTRY_CLIENTINFO);
   remove_proc_entry(entryNameBuf, NULL);

   scnprintf(entryNameBuf, BEEGFS_PROC_NAMEBUF_LEN, "%s/%s",
      dirNameBuf, BEEGFS_PROC_ENTRY_METATARGETSTATES);
   remove_proc_entry(entryNameBuf, NULL);

   scnprintf(entryNameBuf, BEEGFS_PROC_NAMEBUF_LEN, "%s/%s",
      dirNameBuf, BEEGFS_PROC_ENTRY_STORAGETARGETSTATES);
   remove_proc_entry(entryNameBuf, NULL);

   scnprintf(entryNameBuf, BEEGFS_PROC_NAMEBUF_LEN, "%s/%s",
      dirNameBuf, BEEGFS_PROC_ENTRY_RETRIESENABLED);
   remove_proc_entry(entryNameBuf, NULL);

   scnprintf(entryNameBuf, BEEGFS_PROC_NAMEBUF_LEN, "%s/%s",
      dirNameBuf, BEEGFS_PROC_ENTRY_REMAPCONNFAILURE);
   remove_proc_entry(entryNameBuf, NULL);

   scnprintf(entryNameBuf, BEEGFS_PROC_NAMEBUF_LEN, "%s/%s",
      dirNameBuf, BEEGFS_PROC_ENTRY_NETBENCHENABLED);
   remove_proc_entry(entryNameBuf, NULL);

   scnprintf(entryNameBuf, BEEGFS_PROC_NAMEBUF_LEN, "%s/%s",
      dirNameBuf, BEEGFS_PROC_ENTRY_DROPCONNS);
   remove_proc_entry(entryNameBuf, NULL);

   scnprintf(entryNameBuf, BEEGFS_PROC_NAMEBUF_LEN, "%s/%s",
      dirNameBuf, BEEGFS_PROC_ENTRY_LOGLEVELS);
   remove_proc_entry(entryNameBuf, NULL);


   // remove unique dir
   remove_proc_entry(dirNameBuf, NULL);


   SAFE_VFREE(dirNameBuf);
   SAFE_VFREE(entryNameBuf);
}

/**
 * called when a procfs file is being opened.
 *
 * this method handles the assignment of the corresponding readV2 method for a certain entry.
 */
int __ProcFs_open(struct inode* inode, struct file* file)
{
   int (*show)(struct seq_file *, void *) = __ProcFs_getProcDirEntryDataField(inode);
   App* app = __ProcFs_getProcParentDirEntryDataField(inode); // (app is ->data in parent dir)

   return single_open(file, show, app);
}

/**
 * Does not return anything to the reading process.
 * Intended for proc entries that are write-only.
 */
int __ProcFs_readV2_nothing(struct seq_file* file, void* p)
{
   return 0;
}


/**
 * Does not return anything to the reading process.
 * Intended for proc entries that are write-only.
 *
 * @param data specified at entry creation
 */
int ProcFs_read_nothing(char* buf, char** start, off_t offset, int size, int* eof, void* data)
{
   *eof = 1;
   return 0;
}


int __ProcFs_readV2_config(struct seq_file* file, void* p)
{
   App* app = file->private;

   return ProcFsHelper_readV2_config(file, app);
}

/**
 * @param data specified at entry creation
 */
int ProcFs_read_config(char* buf, char** start, off_t offset, int size, int* eof, void* data)
{
   return ProcFsHelper_read_config(buf, start, offset, size, eof, (App*)data);
}

int __ProcFs_readV2_status(struct seq_file* file, void* v)
{
   App* app = file->private;

   return ProcFsHelper_readV2_status(file, app);
}

/**
 * @param data specified at entry creation
 */
int ProcFs_read_status(char* buf, char** start, off_t offset, int size, int* eof, void* data)
{
   return ProcFsHelper_read_status(buf, start, offset, size, eof, (App*)data);
}

int __ProcFs_readV2_mgmtNodes(struct seq_file* file, void* p)
{
   App* app = file->private;
   NodeStoreEx* nodes = App_getMgmtNodes(app);

   return ProcFsHelper_readV2_nodes(file, app, nodes);
}

/**
 * @param data specified at entry creation
 */
int ProcFs_read_mgmtNodes(char* buf, char** start, off_t offset, int size, int* eof, void* data)
{
   return ProcFsHelper_read_nodes(buf, start, offset, size, eof,
      App_getMgmtNodes( (App*)data) );
}

int __ProcFs_readV2_metaNodes(struct seq_file* file, void* p)
{
   App* app = file->private;
   NodeStoreEx* nodes = App_getMetaNodes(app);

   return ProcFsHelper_readV2_nodes(file, app, nodes);
}

/**
 * @param data specified at entry creation
 */
int ProcFs_read_metaNodes(char* buf, char** start, off_t offset, int size, int* eof, void* data)
{
   return ProcFsHelper_read_nodes(buf, start, offset, size, eof,
      App_getMetaNodes( (App*)data) );
}

int __ProcFs_readV2_storageNodes(struct seq_file* file, void* p)
{
   App* app = file->private;
   NodeStoreEx* nodes = App_getStorageNodes(app);

   return ProcFsHelper_readV2_nodes(file, app, nodes);
}

/**
 * @param data specified at entry creation
 */
int ProcFs_read_storageNodes(char* buf, char** start, off_t offset, int size, int* eof, void* data)
{
   return ProcFsHelper_read_nodes(buf, start, offset, size, eof,
      App_getStorageNodes( (App*)data) );
}

int __ProcFs_readV2_clientInfo(struct seq_file* file, void* p)
{
   App* app = file->private;

   return ProcFsHelper_readV2_clientInfo(file, app);
}

int ProcFs_read_clientInfo(char* buf, char** start, off_t offset, int size, int* eof,
   void* data)
{
   return ProcFsHelper_read_clientInfo(buf, start, offset, size, eof, (App*)data);
}

int __ProcFs_readV2_metaTargetStates(struct seq_file* file, void* p)
{
   App* app = file->private;
   TargetStateStore* metaStates = App_getMetaStateStore(app);
   NodeStoreEx* nodes = App_getMetaNodes(app);

   return ProcFsHelper_readV2_targetStates(file, app, metaStates, nodes, true);
}

/**
 * @param data specified at entry creation
 */
int ProcFs_read_metaTargetStates(char* buf, char** start, off_t offset, int size, int* eof,
   void* data)
{
   return ProcFsHelper_read_targetStates(buf, start, offset, size, eof, (App*)data,
      App_getMetaStateStore( (App*)data), App_getMetaNodes( (App*)data), true);
}

int __ProcFs_readV2_storageTargetStates(struct seq_file* file, void* p)
{
   App* app = file->private;
   TargetStateStore* targetStates = App_getTargetStateStore(app);
   NodeStoreEx* nodes = App_getStorageNodes(app);

   return ProcFsHelper_readV2_targetStates(file, app, targetStates, nodes, false);
}

/**
 * @param data specified at entry creation
 */
int ProcFs_read_storageTargetStates(char* buf, char** start, off_t offset, int size, int* eof,
   void* data)
{
   return ProcFsHelper_read_targetStates(buf, start, offset, size, eof, (App*)data,
      App_getTargetStateStore( (App*)data), App_getStorageNodes( (App*)data), false);
}

int __ProcFs_readV2_connRetriesEnabled(struct seq_file* file, void* p)
{
   App* app = file->private;

   return ProcFsHelper_readV2_connRetriesEnabled(file, app);
}

int ProcFs_read_connRetriesEnabled(char* buf, char** start, off_t offset, int size, int* eof,
   void* data)
{
   return ProcFsHelper_read_connRetriesEnabled(buf, start, offset, size, eof, (App*)data);
}

/**
 * @param data specified at entry creation
 */
ssize_t __ProcFs_writeV2_connRetriesEnabled(struct file *file, const char __user *buf,
   size_t count, loff_t *ppos)
{
   struct inode* procInode = file_inode(file);
   App* app = __ProcFs_getProcParentDirEntryDataField(procInode); // (app is ->data in parent dir)

   // check user buffer
   if(unlikely(!os_access_ok(VERIFY_READ, buf, count) ) )
      return -EFAULT;

   return ProcFsHelper_write_connRetriesEnabled(buf, count, app);
}

int ProcFs_write_connRetriesEnabled(struct file* file, const char __user *buf,
   unsigned long count, void* data)
{
   // check user buffer
   if(unlikely(!os_access_ok(VERIFY_READ, buf, count) ) )
      return -EFAULT;

   return ProcFsHelper_write_connRetriesEnabled(buf, count, (App*)data);
}

int __ProcFs_read_remapConnectionFailure(struct seq_file* file, void* p)
{
   App* app = file->private;

   return ProcFsHelper_read_remapConnectionFailure(file, app);
}

ssize_t __ProcFs_write_remapConnectionFailure(struct file *file, const char __user *buf,
   size_t count, loff_t *ppos)
{
   struct inode* procInode = file_inode(file);
   App* app = __ProcFs_getProcParentDirEntryDataField(procInode); // (app is ->data in parent dir)

   // check user buffer
   if(unlikely(!os_access_ok(VERIFY_READ, buf, count) ) )
      return -EFAULT;

   return ProcFsHelper_write_remapConnectionFailure(buf, count, app);
}

int __ProcFs_readV2_netBenchModeEnabled(struct seq_file* file, void* p)
{
   App* app = file->private;

   return ProcFsHelper_readV2_netBenchModeEnabled(file, app);
}

/**
 * @param data specified at entry creation
 */
int ProcFs_read_netBenchModeEnabled(char* buf, char** start, off_t offset, int size, int* eof,
   void* data)
{
   return ProcFsHelper_read_netBenchModeEnabled(buf, start, offset, size, eof, (App*)data);
}

ssize_t __ProcFs_writeV2_netBenchModeEnabled(struct file *file, const char __user *buf,
   size_t count, loff_t *ppos)
{
   struct inode* procInode = file_inode(file);
   App* app = __ProcFs_getProcParentDirEntryDataField(procInode); // (app is ->data in parent dir)

   // check user buffer
   if(unlikely(!os_access_ok(VERIFY_READ, buf, count) ) )
      return -EFAULT;

   return ProcFsHelper_write_netBenchModeEnabled(buf, count, app);
}

/**
 * @param data specified at entry creation
 */
int ProcFs_write_netBenchModeEnabled(struct file* file, const char __user *buf,
   unsigned long count, void* data)
{
   // check user buffer
   if(unlikely(!os_access_ok(VERIFY_READ, buf, count) ) )
      return -EFAULT;

   return ProcFsHelper_write_netBenchModeEnabled(buf, count, (App*)data);
}

ssize_t __ProcFs_writeV2_dropConns(struct file *file, const char __user *buf,
   size_t count, loff_t *ppos)
{
   struct inode* procInode = file_inode(file);
   App* app = __ProcFs_getProcParentDirEntryDataField(procInode); // (app is ->data in parent dir)

   // check user buffer
   if(unlikely(!os_access_ok(VERIFY_READ, buf, count) ) )
      return -EFAULT;

   return ProcFsHelper_write_dropConns(buf, count, app);
}

/**
 * @param data specified at entry creation
 */
int ProcFs_write_dropConns(struct file* file, const char __user *buf,
   unsigned long count, void* data)
{
   // check user buffer
   if(unlikely(!os_access_ok(VERIFY_READ, buf, count) ) )
      return -EFAULT;

   return ProcFsHelper_write_dropConns(buf, count, (App*)data);
}

int __ProcFs_readV2_logLevels(struct seq_file* file, void* p)
{
   App* app = file->private;

   return ProcFsHelper_readV2_logLevels(file, app);
}

/**
 * @param data specified at entry creation
 */
int ProcFs_read_logLevels(char* buf, char** start, off_t offset, int size, int* eof,
   void* data)
{
   return ProcFsHelper_read_logLevels(buf, start, offset, size, eof, (App*)data);
}

ssize_t __ProcFs_writeV2_logLevels(struct file *file, const char __user *buf,
   size_t count, loff_t *ppos)
{
   struct inode* procInode = file_inode(file);
   App* app = __ProcFs_getProcParentDirEntryDataField(procInode); // (app is ->data in parent dir)

   // check user buffer
   if(unlikely(!os_access_ok(VERIFY_READ, buf, count) ) )
      return -EFAULT;

   return ProcFsHelper_write_logLevels(buf, count, app);
}

/**
 * @param data specified at entry creation
 */
int ProcFs_write_logLevels(struct file* file, const char __user *buf,
   unsigned long count, void* data)
{
   // check user buffer
   if(unlikely(!os_access_ok(VERIFY_READ, buf, count) ) )
      return -EFAULT;

   return ProcFsHelper_write_logLevels(buf, count, (App*)data);
}

/**
 * Create a proc dir.
 *
 * Note: This is actually just a compat method for proc_mkdir_data.
 *
 * @param data arbitrary private value to be assigned to procDir->data
 */
struct proc_dir_entry* __ProcFs_mkDir(const char* name, void* data)
{
   /* newer kernels do no longer export struct proc_dir_entry, so the data field is only
      accessible through special kernel methods. */

   struct proc_dir_entry* procDir;


   #if defined(KERNEL_HAS_PDE_DATA) || defined(KERNEL_HAS_NEW_PDE_DATA)

      procDir = proc_mkdir_data(name, 0, NULL, data);

   #else

      procDir = proc_mkdir(name, NULL);

      if(procDir)
         procDir->data = data;

   #endif // KERNEL_HAS_PDE_DATA


   return procDir;
}

/**
 * Return the data field of a proc entry.
 *
 * Note: This is actually just a compat method for PDE_DATA.
 */
void* __ProcFs_getProcDirEntryDataField(const struct inode* procInode)
{
   /* newer kernels do no longer export struct proc_dir_entry, so the data field is only
      accessible through special kernel methods. */

   #ifdef KERNEL_HAS_PDE_DATA

      return PDE_DATA(procInode);

   #elif defined(KERNEL_HAS_NEW_PDE_DATA)

      return pde_data(procInode);

   #else

      struct proc_dir_entry* procEntry = PDE(procInode);

      return procEntry->data; // (app is stored as ->data in parent dir)

   #endif // KERNEL_HAS_PDE_DATA
}

/**
 * Return the data field from the parent dir of a proc entry.
 *
 * Note: This is actually just a compat method for proc_get_parent_data.
 */
void* __ProcFs_getProcParentDirEntryDataField(const struct inode* procInode)
{
   /* newer kernels do no longer export struct proc_dir_entry, so the ->parent and ->data fields are
      only accessible through special kernel methods. */

   #if defined(KERNEL_HAS_PDE_DATA) || defined(KERNEL_HAS_NEW_PDE_DATA)

      return proc_get_parent_data(procInode);

   #else

      struct proc_dir_entry* procEntry = PDE(procInode);

      return procEntry->parent->data; // (app is stored as ->data in parent dir)

   #endif // KERNEL_HAS_PDE_DATA
}
