#ifndef IBVBuffer_h_aMQFNfzrjbEHDOcv216fi
#define IBVBuffer_h_aMQFNfzrjbEHDOcv216fi

#include <common/net/sock/rdma_autoconf.h>

#ifdef BEEGFS_OPENTK_IBVERBS

#include <common/Common.h>
#include <os/iov_iter.h>

#ifndef BEEGFS_IBVERBS_FRAGMENT_PAGES
#define BEEGFS_IBVERBS_FRAGMENT_PAGES 1
#endif

#define IBV_FRAGMENT_SIZE ((BEEGFS_IBVERBS_FRAGMENT_PAGES) * PAGE_SIZE)


struct IBVBuffer;
typedef struct IBVBuffer IBVBuffer;

struct IBVCommContext;


extern bool IBVBuffer_init(IBVBuffer* buffer, struct IBVCommContext* ctx, size_t bufLen);
extern void IBVBuffer_free(IBVBuffer* buffer, struct IBVCommContext* ctx);
extern ssize_t IBVBuffer_fill(IBVBuffer* buffer, struct iov_iter* iter);


struct IBVBuffer
{
   char** buffers;
   struct ib_sge* lists;

   size_t bufferSize;
   unsigned bufferCount;

   unsigned listLength;
};

#endif

#endif
