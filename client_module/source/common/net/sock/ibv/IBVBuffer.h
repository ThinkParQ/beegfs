#ifndef IBVBuffer_h_aMQFNfzrjbEHDOcv216fi
#define IBVBuffer_h_aMQFNfzrjbEHDOcv216fi

#include <common/Common.h>
#ifdef BEEGFS_RDMA

#include <rdma/ib_verbs.h>
#include <rdma/rdma_cm.h>
#include <rdma/ib_cm.h>

#include <os/iov_iter.h>

#ifndef BEEGFS_IBVERBS_FRAGMENT_PAGES
#define BEEGFS_IBVERBS_FRAGMENT_PAGES 1
#endif

#define IBV_FRAGMENT_SIZE ((BEEGFS_IBVERBS_FRAGMENT_PAGES) * PAGE_SIZE)


struct IBVBuffer;
typedef struct IBVBuffer IBVBuffer;

struct IBVCommContext;


extern bool IBVBuffer_init(IBVBuffer* buffer, struct IBVCommContext* ctx, size_t bufLen,
   enum dma_data_direction dma_dir);
extern void IBVBuffer_free(IBVBuffer* buffer, struct IBVCommContext* ctx);
extern ssize_t IBVBuffer_fill(IBVBuffer* buffer, struct iov_iter* iter);


struct IBVBuffer
{
   char** buffers;
   struct ib_sge* lists;

   size_t bufferSize;
   unsigned bufferCount;

   unsigned listLength;
   enum dma_data_direction dma_dir;
};

#endif

#endif
