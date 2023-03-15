

#include "IBVBuffer.h"
#include "IBVSocket.h"
#ifdef BEEGFS_RDMA

bool IBVBuffer_init(IBVBuffer* buffer, IBVCommContext* ctx, size_t bufLen,
    enum dma_data_direction dma_dir)
{
   unsigned count = (bufLen + IBV_FRAGMENT_SIZE - 1) / IBV_FRAGMENT_SIZE;
   unsigned i;

   bufLen = MIN(IBV_FRAGMENT_SIZE, bufLen);

   buffer->dma_dir = dma_dir;
   buffer->buffers = kzalloc(count * sizeof(*buffer->buffers), GFP_KERNEL);
   buffer->lists = kzalloc(count * sizeof(*buffer->lists), GFP_KERNEL);
   if(!buffer->buffers || !buffer->lists)
      goto fail;

   for(i = 0; i < count; i++)
   {
#ifndef OFED_UNSAFE_GLOBAL_RKEY
      buffer->lists[i].lkey = ctx->dmaMR->lkey;
#else
      buffer->lists[i].lkey = ctx->pd->local_dma_lkey;
#endif
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

   if (buffer->buffers)
      kfree(buffer->buffers);

   if (buffer->lists)
      kfree(buffer->lists);
}

ssize_t IBVBuffer_fill(IBVBuffer* buffer, struct iov_iter* iter)
{
   ssize_t total = 0;
   unsigned i;

   for(i = 0; i < buffer->bufferCount && iter->count > 0; i++)
   {
      size_t fragment = MIN(MIN(iter->count, buffer->bufferSize), 0xFFFFFFFF);

      if(copy_from_iter(buffer->buffers[i], fragment, iter) != fragment)
         return -EFAULT;

      buffer->lists[i].length = fragment;
      buffer->listLength = i + 1;

      total += fragment;
   }

   return total;
}

#endif
