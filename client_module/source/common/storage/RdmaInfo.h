#ifndef _RDMAINFO_H_
#define _RDMAINFO_H_
#ifdef BEEGFS_NVFS

#include "linux/uio.h"
#include <common/net/sock/RDMASocket.h>
#include <common/toolkit/Serialization.h>
#include <common/storage/Nvfs.h>

#define RDMA_MAX_DMA_COUNT   64

struct RdmaInfo
{
   size_t              sg_count;
   size_t              dma_count;
   uint32_t            tag;
   uint64_t            key;
   struct ib_device   *device;
   void               *pages;
   struct scatterlist *sglist;
   struct scatterlist *dmalist;
};
typedef struct RdmaInfo RdmaInfo;

static inline void RdmaInfo_serialize(SerializeCtx* ctx, RdmaInfo *rdmap)
{
   int i = 0;
   struct scatterlist *sgp;

   if (rdmap != NULL)
   {
      Serialization_serializeUInt64(ctx, rdmap->dma_count);

      if (rdmap->dma_count > 0)
      {
         Serialization_serializeUInt(ctx, rdmap->tag);
         Serialization_serializeUInt64(ctx, rdmap->key);
         for (i = 0, sgp = rdmap->dmalist; i < rdmap->dma_count; i++, sgp++)
         {
            Serialization_serializeUInt64(ctx, sg_dma_address(sgp));
            Serialization_serializeUInt64(ctx, (uint64_t)sgp->length);
            Serialization_serializeUInt64(ctx, (uint64_t)sgp->offset);
         }
      }
   }
   else
   {
      Serialization_serializeUInt64(ctx, 0ull);
   }
}

bool RdmaInfo_acquireNVFS(void);
void RdmaInfo_releaseNVFS(void);
int RdmaInfo_detectNVFSRequest(DevicePriorityContext* dpctx,
   const struct iov_iter *iter);
static inline int RdmaInfo_nvfsDevicePriority(struct ib_device* dev,
   int gpuIndex);

RdmaInfo* RdmaInfo_mapRead(const struct iov_iter *iter, Socket *socket);
RdmaInfo* RdmaInfo_mapWrite(const struct iov_iter *iter, Socket *socket);
void RdmaInfo_unmapRead(RdmaInfo *rdmap);
void RdmaInfo_unmapWrite(RdmaInfo *rdmap);

#ifdef BEEGFS_DEBUG_RDMA
static inline void RdmaInfo_dumpIovIter(const struct iov_iter *iter)
{
   int             i = 0;
   struct iov_iter  iter_copy = *iter;

   printk(KERN_ALERT "IOV_ITER : count=%ld", iov_iter_count(&iter_copy));
   while (iov_iter_count(&iter_copy) > 0)
   {
      printk(KERN_INFO "      %3d: %px %ld", i, iter_iov_addr(&iter_copy), iter_iov_len(&iter_copy));
      iov_iter_advance(&iter_copy, iter_iov_len(&iter_copy));
   }
}

static inline void RdmaInfo_dumpSgtable(const char *header,
   struct scatterlist *sglist, size_t sg_count)
{
   int                 i = 0;
   struct scatterlist *sgp = NULL;

   printk_fhgfs(KERN_INFO, "%-12s : (%ld entries)", header, sg_count);
   for (i = 0, sgp = sglist; i < sg_count; i++, sgp++)
   {
      printk_fhgfs(KERN_INFO, "       [%3d] = %016llx %d %d %d",
         i,
         sg_dma_address(sgp),
         sg_dma_len(sgp),
         sgp->length,
         sgp->offset);
   }
}

static inline void RdmaInfo_dumpRdmaInfo(RdmaInfo *rdmap)
{
   if (rdmap != NULL)
   {
      printk_fhgfs(KERN_INFO, "RDMA :\n"
         "       NENTS = %ld\n"
         "         TAG = %x\n"
         "         KEY = %llx\n",
         rdmap->dma_count, rdmap->tag, rdmap->key);
      RdmaInfo_dumpSgtable("SGLIST", rdmap->sglist, rdmap->sg_count);
      RdmaInfo_dumpSgtable("DMALIST", rdmap->dmalist, rdmap->dma_count);
   }
}
#endif /* BEEGFS_DEBUG_RDMA */

int RdmaInfo_nvfsDevicePriority(struct ib_device* dev, int gpuIndex)
{
   return nvfs_ops->nvfs_device_priority(dev->dma_device, gpuIndex);
}

#endif /* BEEGFS_NVFS */
#endif /* _RDMAINFO_H_ */
