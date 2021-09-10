#ifndef PROCFSHELPER_H_
#define PROCFSHELPER_H_

#include <app/App.h>
#include <common/Common.h>
#include <nodes/NodeStoreEx.h>

#include <linux/seq_file.h>


extern int ProcFsHelper_readV2_config(struct seq_file* file, App* app);
extern int ProcFsHelper_readV2_status(struct seq_file* file, App* app);
extern int ProcFsHelper_readV2_nodes(struct seq_file* file, App* app, struct NodeStoreEx* nodes);
extern int ProcFsHelper_readV2_clientInfo(struct seq_file* file, App* app);
extern int ProcFsHelper_readV2_targetStates(struct seq_file* file, App* app,
   struct TargetStateStore* targetStates, struct NodeStoreEx* nodes, bool isMeta);
extern int ProcFsHelper_readV2_connRetriesEnabled(struct seq_file* file, App* app);
extern int ProcFsHelper_readV2_netBenchModeEnabled(struct seq_file* file, App* app);
extern int ProcFsHelper_readV2_logLevels(struct seq_file* file, App* app);

extern int ProcFsHelper_read_config(char* buf, char** start, off_t offset, int size, int* eof,
   App* app);
extern int ProcFsHelper_read_status(char* buf, char** start, off_t offset, int size, int* eof,
   App* app);
extern int ProcFsHelper_read_nodes(char* buf, char** start, off_t offset, int size, int* eof,
   struct NodeStoreEx* nodes);
extern int ProcFsHelper_read_clientInfo(char* buf, char** start, off_t offset, int size, int* eof,
   App* app);
extern int ProcFsHelper_read_targetStates(char* buf, char** start, off_t offset, int size, int* eof,
   App* app, struct TargetStateStore* targetStates, struct NodeStoreEx* nodes, bool isMeta);
extern int ProcFsHelper_read_connRetriesEnabled(char* buf, char** start, off_t offset, int size,
   int* eof, App* app);
extern int ProcFsHelper_write_connRetriesEnabled(const char __user *buf,
   unsigned long count, App* app);

extern int ProcFsHelper_read_remapConnectionFailure(struct seq_file* file, App* app);
extern int ProcFsHelper_write_remapConnectionFailure(const char __user *buf, unsigned long count, App* app);

extern int ProcFsHelper_read_netBenchModeEnabled(char* buf, char** start, off_t offset, int size,
   int* eof, App* app);
extern int ProcFsHelper_write_netBenchModeEnabled(const char __user *buf,
   unsigned long count, App* app);
extern int ProcFsHelper_write_dropConns(const char __user *buf, unsigned long count, App* app);
extern int ProcFsHelper_read_logLevels(char* buf, char** start, off_t offset, int size, int* eof,
   App* app);
extern int ProcFsHelper_write_logLevels(const char __user *buf, unsigned long count, App* app);

extern void __ProcFsHelper_printGotRootV2(struct seq_file* file, Node* node, NodeStoreEx* nodes);
extern void __ProcFsHelper_printGotRoot(struct Node* node, struct NodeStoreEx* nodes, char* buf,
   int* pcount, int* psize);
extern void __ProcFsHelper_printNodeConnsV2(struct seq_file* file, struct Node* node);
extern void __ProcFsHelper_printNodeConns(struct Node* node, char* buf, int* pcount, int* psize);

extern const char* ProcFsHelper_getSessionID(App* app);

#endif /* PROCFSHELPER_H_ */
