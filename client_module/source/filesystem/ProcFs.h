#ifndef PROCFS_H_
#define PROCFS_H_

#include <common/nodes/TargetStateStore.h>
#include <common/Common.h>
#include <app/App.h>

#include <linux/proc_fs.h>
#include <linux/seq_file.h>


extern void ProcFs_createGeneralDir(void);
extern void ProcFs_removeGeneralDir(void);
extern void ProcFs_createEntries(App* app);
extern void ProcFs_removeEntries(App* app);

extern int __ProcFs_open(struct inode* inode, struct file* file);

extern int __ProcFs_readV2_nothing(struct seq_file* file, void* p);
extern int __ProcFs_readV2_config(struct seq_file* file, void* p);
extern int __ProcFs_readV2_status(struct seq_file* file, void* p);
extern int __ProcFs_readV2_mgmtNodes(struct seq_file* file, void* p);
extern int __ProcFs_readV2_metaNodes(struct seq_file* file, void* p);
extern int __ProcFs_readV2_storageNodes(struct seq_file* file, void* p);
extern int __ProcFs_readV2_clientInfo(struct seq_file* file, void* p);
extern int __ProcFs_readV2_metaTargetStates(struct seq_file* file, void* p);
extern int __ProcFs_readV2_storageTargetStates(struct seq_file* file, void* p);

extern int __ProcFs_readV2_connRetriesEnabled(struct seq_file* file, void* p);
extern int __ProcFs_readV2_netBenchModeEnabled(struct seq_file* file, void* p);
extern int __ProcFs_readV2_logLevels(struct seq_file* file, void* p);

extern ssize_t __ProcFs_writeV2_connRetriesEnabled(struct file *file, const char __user *buf,
   size_t count, loff_t *ppos);
extern ssize_t __ProcFs_writeV2_netBenchModeEnabled(struct file *file, const char __user *buf,
   size_t count, loff_t *ppos);
extern ssize_t __ProcFs_writeV2_dropConns(struct file *file, const char __user *buf,
   size_t count, loff_t *ppos);
extern ssize_t __ProcFs_writeV2_logLevels(struct file *file, const char __user *buf,
   size_t count, loff_t *ppos);

extern int __ProcFs_read_remapConnectionFailure(struct seq_file* file, void* p);
extern ssize_t __ProcFs_write_remapConnectionFailure(struct file *file, const char __user *buf,
   size_t count, loff_t *ppos);

extern struct proc_dir_entry* __ProcFs_mkDir(const char* name, void* data);
extern void* __ProcFs_getProcDirEntryDataField(const struct inode* procInode);
extern void* __ProcFs_getProcParentDirEntryDataField(const struct inode* procInode);

#endif /* PROCFS_H_ */
