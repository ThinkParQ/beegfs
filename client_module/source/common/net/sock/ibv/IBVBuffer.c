

#include "IBVBuffer.h"
#include "IBVSocket.h"
#ifdef BEEGFS_RDMA
#include <rdma/ib_verbs.h>


bool IBVBuffer_init(IBVBuffer* buffer, IBVCommContext* ctx, size_t bufLen,
   size_t fragmentLen, enum dma_data_direction dma_dir)
{
   unsigned count;
   unsigned i;

   if (fragmentLen == 0)
      fragmentLen = bufLen;
   count = (bufLen + fragmentLen - 1) / fragmentLen;
   bufLen = MIN(fragmentLen, bufLen);

   buffer->dma_dir = dma_dir;
   buffer->buffers = kzalloc(count * sizeof(*buffer->buffers), GFP_KERNEL);
   buffer->lists = kzalloc(count * sizeof(*buffer->lists), GFP_KERNEL);
   if(!buffer->buffers || !buffer->lists)
      goto fail;

   for(i = 0; i < count; i++)
   {
      buffer->lists[i].lkey = ctx->pd->local_dma_lkey;
      buffer->lists[i].length = bufLen;
      buffer->buffers[i] = kmalloc(bufLen, GFP_KERNEL);
      if(unlikely(!buffer->buffers[i]))
      {
         printk_fhgfs(KERN_ERR, "Failed to allocate buffer size=%zu\n", bufLen);
         goto fail;
      }
      buffer->lists[i].addr = ib_dma_map_single(ctx->pd->device, buffer->buffers[i],
         bufLen, dma_dir);
      if (unlikely(ib_dma_mapping_error(ctx->pd->device, buffer->lists[i].addr)))
      {
         buffer->lists[i].addr = 0;
         printk_fhgfs(KERN_ERR, "Failed to dma map buffer size=%zu\n", bufLen);
         goto fail;
      }
      BUG_ON(buffer->lists[i].addr == 0);
   }

   buffer->bufferSize = bufLen;
   buffer->listLength = count;
   buffer->bufferCount = count;
   return true;

fail:
   IBVBuffer_free(buffer, ctx);
   return false;
}


bool IBVBuffer_initRegistration(IBVBuffer* buffer, IBVCommContext* ctx)
{
   struct scatterlist* sg;
   int res;
   int i;

   buffer->mr = ib_alloc_mr(ctx->pd, IB_MR_TYPE_MEM_REG, buffer->bufferCount);
   if (IS_ERR(buffer->mr))
   {
      printk_fhgfs(KERN_ERR, "Failed to alloc mr, errCode=%ld\n", PTR_ERR(buffer->mr));
      buffer->mr = NULL;
      goto fail;
   }

   sg = kzalloc(buffer->bufferCount * sizeof(struct scatterlist), GFP_KERNEL);
   if (sg == NULL)
   {
      printk_fhgfs(KERN_ERR, "Failed to alloc sg\n");
      goto fail;
   }

   for (i = 0; i < buffer->bufferCount; ++i)
   {
      sg_dma_address(&sg[i]) = buffer->lists[i].addr;
      sg_dma_len(&sg[i]) = buffer->lists[i].length;
   }

   res = ib_map_mr_sg(buffer->mr, sg, buffer->bufferCount, NULL, PAGE_SIZE);
   kfree(sg);
   if (res < 0)
   {
      printk_fhgfs(KERN_ERR, "Failed to map mr res=%d\n", res);
      goto fail;
   }

   return true;

fail:
   if (buffer->mr)
   {
      ib_dereg_mr(buffer->mr);
      buffer->mr = NULL;
   }
   return false;
}


void IBVBuffer_free(IBVBuffer* buffer, IBVCommContext* ctx)
{
   if(buffer->buffers && buffer->lists)
   {
      unsigned i;
      for(i = 0; i < buffer->bufferCount; i++)
      {
         if (buffer->lists[i].addr)
            ib_dma_unmap_single(ctx->pd->device, buffer->lists[i].addr,
               buffer->bufferSize, buffer->dma_dir);

         if (buffer->buffers[i])
            kfree(buffer->buffers[i]);
      }
   }

   if (buffer->mr)
      ib_dereg_mr(buffer->mr);

   if (buffer->buffers)
      kfree(buffer->buffers);

   if (buffer->lists)
      kfree(buffer->lists);
}

ssize_t IBVBuffer_fill(IBVBuffer* buffer, struct iov_iter* iter)
{
   ssize_t total = 0;
   unsigned i;

   for(i = 0; i < buffer->bufferCount && iov_iter_count(iter) > 0; i++)
   {
      size_t fragment = MIN(MIN(iov_iter_count(iter), buffer->bufferSize), 0xFFFFFFFF);

      if(copy_from_iter(buffer->buffers[i], fragment, iter) != fragment)
         return -EFAULT;

      buffer->lists[i].length = fragment;
      buffer->listLength = i + 1;

      total += fragment;
   }

   return total;
}

#endif
