#ifdef BEEGFS_NVFS
#include "linux/uio.h"
#include "linux/pagemap.h"
#include "linux/kernel.h"
#include <rdma/ib_verbs.h>
#include <rdma/rdma_cm.h>
#include <rdma/ib_cm.h>
#include <common/net/sock/RDMASocket.h>
#include "Nvfs.h"
#include "RdmaInfo.h"

//
// These macros convert a scatterlist entry into a base address (ba) and limit address (la)
// and vice-versa.  From this information, we can combine scatterlist entries which are DMA
// contiguous.
//
#define sg_to_ba_la(SG, BA, LA)                 \
   do                                           \
   {                                            \
      BA = sg_dma_address(SG);                  \
      LA = BA + sg_dma_len(SG);                 \
   } while (0)

#define ba_la_to_sg(SG, BA, LA)                 \
   do                                           \
   {                                            \
      sg_dma_address(SG) = BA;                  \
      sg_dma_len(SG) = LA - BA;                 \
      SG->length = LA - BA;                     \
   } while (0)

bool RdmaInfo_acquireNVFS(void)
{
#ifdef BEEGFS_DEBUG
   printk_fhgfs(KERN_INFO, "%s\n", __func__);
#endif // BEEGFS_DEBUG
   return nvfs_get_ops();
}

void RdmaInfo_releaseNVFS(void)
{
#ifdef BEEGFS_DEBUG
   printk_fhgfs(KERN_INFO, "%s\n", __func__);
#endif // BEEGFS_DEBUG
   nvfs_put_ops();
}

int RdmaInfo_detectNVFSRequest(DevicePriorityContext* dpctx,
   const struct iov_iter *iter)
{
   struct page     *page = NULL;
   struct iov_iter  iter_copy = *iter;
   size_t           page_offset = 0;
   int              status = 0;
   bool             is_gpu = false;

   // Test the first page of the request to determine the memory type.
   status = iov_iter_get_pages(&iter_copy, &page, PAGE_SIZE, 1, &page_offset);
   if (unlikely(status <= 0))
   {
      // 0 means the iter is empty, so just indicate that it's not an NVFS call.
      // Otherwise, indicate an error condition.if (unlikely(status <= 0))
      if (status < 0)
         printk_fhgfs(KERN_WARNING, "%s: can't retrieve page from iov_iter, status=%d\n",
            __func__, status);
      return status == 0? false : status;
   }

   // At this point, the request did come in through nvidia_fs.
   // nvfs_is_gpu_page() will return false if RDMA write support
   // is disabled in user space.
   // TODO: if a GPU page, keep the retrieved page for a future
   // RDMA map operation instead of calling put_page()
   if (nvfs_ops->nvfs_is_gpu_page(page))
   {
      is_gpu = true;
      dpctx->gpuIndex = nvfs_ops->nvfs_gpu_index(page);
   }
   put_page(page);
#ifdef BEEGFS_DEBUG
   printk_fhgfs(KERN_INFO, "%s:%d: page=%p is_gpu=%d gpu_index=%d\n",
      __func__, __LINE__,
      page, is_gpu, dpctx? dpctx->gpuIndex : -2);
#endif
   return is_gpu;
}


/*
 * RdmaInfo_putPpages - Put the pages back into free list.
 * @sglist: The array of scatter/gather entries
 * @count: The count of entries to process
 */
static inline void RdmaInfo_putPages(struct scatterlist *sglist, int count)
{
   int i = 0;
   struct scatterlist *sgp = NULL;

   if ((sglist != NULL) && (count > 0))
   {
      for (i = 0, sgp = sglist; i < count; i++, sgp++)
      {
         put_page(sg_page(sgp));
      }
   }
}

/*
 * RdmaInfo_iovToSglist - Map an iov_iter to scatter/gather list
 * @iter: iov_iter
 * @sglist: The array of scatter/gather entries (needs to be big enough for all pages)
 * @returns number of sg entries set up for the iov_iter
 */
static int RdmaInfo_iovToSglist(const struct iov_iter *iter,
   struct scatterlist *sglist)
{
   struct page        **pages = NULL;
   struct scatterlist  *sg = NULL;
   struct scatterlist  *sg_prev = NULL;
   struct iov_iter      iter_copy = *iter;
   int                  sg_count = 0;
   size_t               page_length = 0;
   size_t               page_offset = 0;
   size_t               bytes = 0;
   ssize_t              result = 0;
   unsigned             i = 0;
   unsigned             npages = 0;

   while (iov_iter_count(&iter_copy) > 0)
   {
      result = iov_iter_get_pages_alloc(&iter_copy, &pages, iov_iter_count(&iter_copy), &page_offset);
      if (result < 0)
      {
         printk_fhgfs(KERN_ERR, "RdmaInfo_iovToSglist: no memory pages\n");
         RdmaInfo_putPages(sglist, sg_count);
         return -ENOMEM;
      }

      bytes = result;
      npages = (bytes + page_offset + PAGE_SIZE - 1) / PAGE_SIZE;
      sg_count += npages;

      for (i = 0, sg = sglist; i < npages; i++, sg = sg_next(sg))
      {
         page_length = min(bytes, PAGE_SIZE - page_offset);
         sg_set_page(sg, pages[i], page_length, page_offset);

         bytes -= page_length;
         page_offset = 0;
         sg_prev = sg;
      }

      kvfree(pages);
      iov_iter_advance(&iter_copy, result);
   }

   if (sg_prev)
   {
      sg_mark_end(sg_prev);
   }
   return sg_count;
}

/*
 * RdmaInfo_coalesceSglist - Coalesce scatterlist entries for optimal RDMA operations.
 * @sglist: input list (not necessarily coalesced)
 * @dmalist: output list (coalesced)
 * @count: Number of scatterlist entries
 * @returns count of coalesed list
 */
static int RdmaInfo_coalesceSglist(struct scatterlist *sglist,
   struct scatterlist *dmalist, int count)
{
   struct scatterlist *sgp = sglist;
   struct scatterlist *dmap = dmalist;
   dma_addr_t dma_ba = 0, dma_la = 0;
   dma_addr_t sg_ba = 0, sg_la = 0;
   int i = 0;
#ifdef BEEGFS_DEBUG
   size_t len = sg_dma_len(sgp);
#endif

   //
   // Load the first range.
   //
   sg_to_ba_la(sgp, dma_ba, dma_la);

   if (count > 1)
   {
      for_each_sg(&sglist[1], sgp, count-1, i)
      {
#ifdef BEEGFS_DEBUG
         len += sg_dma_len(sgp);
#endif
         sg_to_ba_la(sgp, sg_ba, sg_la);

         //
         // If the regions aren't contiguous, then set the current
         // range and start a new range.  Otherwise, add on to the
         // current range.
         //
         if (dma_la != sg_ba)
         {
            ba_la_to_sg(dmap, dma_ba, dma_la);
            sg_unmark_end(dmap);
            dmap = sg_next(dmap);

            dma_ba = sg_ba;
            dma_la = sg_la;
         }
         else
         {
            dma_la = sg_la;
         }
      }
   }
   //
   // Set the last range.
   //
   ba_la_to_sg(dmap, dma_ba, dma_la);
   sg_mark_end(dmap);

#ifdef BEEGFS_DEBUG
   printk_fhgfs(KERN_INFO, "%s len=%zu count=%d return=%d\n", __func__, len, count, (int)(1 + dmap - dmalist));
#endif
   return 1 + dmap - dmalist;
}

/*
 * RdmaInfo_map - Map GPU buffers for RDMA operations.
 * @iter: iov_iter
 * @socket: RDMA capable socket struct.
 * @dma_dir: read (DMA_FROM_DEVICE) or write (DMA_TO_DEVICE)
 * @returns RdmaInfo struct
 */
static RdmaInfo * RdmaInfo_map(const struct iov_iter *iter, Socket *socket,
   enum dma_data_direction dma_dir)
{
   RdmaInfo           *rdmap;
   RDMASocket         *rs;
   struct ib_device   *device;
   struct scatterlist *sglist;
   struct scatterlist *dmalist;
   int         status = 0;
   int         sg_count;
   int         dma_count;
   int         count;
   unsigned    npages;
   unsigned    key;

   if (Socket_getSockType(socket) != NICADDRTYPE_RDMA)
      return ERR_PTR(-EINVAL);

   rs = (RDMASocket*) socket;
   if (!RDMASocket_isRkeyGlobal(rs))
   {
      printk_fhgfs(KERN_ERR, "ERROR: rkey type is not compatible with GDS\n");
      return ERR_PTR(-EINVAL);
   }

   npages = 1 + iov_iter_npages(iter, INT_MAX);

   //
   // Allocate the scatterlist.
   //
   rdmap = kzalloc(sizeof(RdmaInfo), GFP_ATOMIC);
   sglist = kzalloc(npages * sizeof(struct scatterlist), GFP_ATOMIC);
   dmalist = kzalloc(npages * sizeof(struct scatterlist), GFP_ATOMIC);
   if (unlikely(!rdmap || !sglist || !dmalist))
   {
      printk_fhgfs(KERN_ERR, "%s: no memory for scatterlist\n", __func__);
      status = -ENOMEM;
      goto error_return;
   }

   //
   // Populate the scatterlist from the iov_iter.
   //
   sg_count = RdmaInfo_iovToSglist(iter, sglist);
   if (unlikely(sg_count < 0))
   {
      printk_fhgfs(KERN_ERR, "%s: can't convert iov_iter to scatterlist\n", __func__);
      status = -EIO;
      goto error_return;
   }

   //
   // DMA map all of the pages.
   //
   device = RDMASocket_getDevice(rs);
   key = RDMASocket_getRkey(rs);

   count = nvfs_ops->nvfs_dma_map_sg_attrs(device->dma_device, sglist, sg_count,
      dma_dir, DMA_ATTR_NO_WARN);
   if (unlikely(count != sg_count))
   {
      if (count == NVFS_CPU_REQ)
      {
         printk_fhgfs(KERN_ERR, "%s: NVFS_CPU_REQ\n", __func__);
         status = 0;
      }
      else if (count == NVFS_IO_ERR)
      {
         printk_fhgfs(KERN_ERR, "%s: can't DMA map mixed CPU/GPU pages\n", __func__);
         status = -EINVAL;
      }
      else
      {
         printk_fhgfs(KERN_ERR, "%s: unknown error returned from NVFS (%d)\n", __func__, count);
         status = -EIO;
      }
      goto error_return;
   }

   //
   // Coalesce the scatterlist.
   //
   dma_count = RdmaInfo_coalesceSglist(sglist, dmalist, count);
   if (unlikely(dma_count > RDMA_MAX_DMA_COUNT))
   {
      printk_fhgfs(KERN_ERR, "%s: too many DMA elements count=%d max=%d\n", __func__,
         dma_count, RDMA_MAX_DMA_COUNT);
      status = -EIO;
      goto error_return;
   }

   //
   // Fill in the rdma info.
   //
   rdmap->dma_count = dma_count;
   rdmap->sg_count = sg_count;
   rdmap->tag = 0x00000000;
   rdmap->device = device;
   rdmap->key = key;
   rdmap->sglist = sglist;
   rdmap->dmalist = dmalist;

#ifdef BEEGFS_DEBUG_RDMA
   RdmaInfo_dumpIovIter(iter);
   RdmaInfo_dumpSgtable("MAP", rdmap->dmalist, rdmap->dma_count);
   RdmaInfo_dumpRdmaInfo(rdmap);
#endif /* BEEGFS_DEBUG_RDMA */

   return rdmap;

error_return:
   if (sglist)
   {
      RdmaInfo_putPages(sglist, sg_count);
      kfree(sglist);
   }
   if (dmalist)
      kfree(dmalist);
   if (rdmap)
      kfree(rdmap);
   return (status == 0) ? NULL : ERR_PTR(status);
}

RdmaInfo* RdmaInfo_mapRead(const struct iov_iter *iter, Socket *socket)
{
   return RdmaInfo_map(iter, socket, DMA_FROM_DEVICE);
}

RdmaInfo* RdmaInfo_mapWrite(const struct iov_iter *iter, Socket *socket)
{
   return RdmaInfo_map(iter, socket, DMA_TO_DEVICE);
}

/*
 * RdmaInfo_unmap - Unmap GPU buffers for RDMA operations.
 * @rdmap: RdmaInfo created by RdmaInfo_map (see above)
 * @dma_dir: read (DMA_FROM_DEVICE) or write (DMA_TO_DEVICE)
 */
static inline void RdmaInfo_unmap(RdmaInfo *rdmap, enum dma_data_direction dma_dir)
{
   if (rdmap->sglist)
   {
      if (rdmap->dmalist)
      {
         nvfs_ops->nvfs_dma_unmap_sg(rdmap->device->dma_device, rdmap->sglist, rdmap->sg_count, dma_dir);
         RdmaInfo_putPages(rdmap->sglist, rdmap->sg_count);
         kfree(rdmap->dmalist);
      }
      kfree(rdmap->sglist);
   }
   kfree(rdmap);
}

void RdmaInfo_unmapRead(RdmaInfo *rdmap)
{
   RdmaInfo_unmap(rdmap, DMA_FROM_DEVICE);
}

void RdmaInfo_unmapWrite(RdmaInfo *rdmap)
{
   RdmaInfo_unmap(rdmap, DMA_TO_DEVICE);
}

#endif   /* BEEGFS_NVFS */
