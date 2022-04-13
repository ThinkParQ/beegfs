#ifndef DEVICEPRIORITYCONTEXT_H_
#define DEVICEPRIORITYCONTEXT_H_

struct DevicePriorityContext
{
   int maxConns;
#ifdef BEEGFS_NVFS
   // index of GPU related to the first page, -1 for none
   int gpuIndex;
#endif
};

typedef struct DevicePriorityContext DevicePriorityContext;

#endif // DEVICEPRIORITYCONTEXT_H_
