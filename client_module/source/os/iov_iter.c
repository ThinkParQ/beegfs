#include <os/iov_iter.h>

#include <linux/mm.h>
#include <linux/slab.h>


static void beegfs_readsink_reserve_no_pipe(BeeGFS_ReadSink *rs, struct iov_iter *iter, size_t size)
{
      rs->sanitized_iter = *iter;
      iov_iter_truncate(&rs->sanitized_iter, size);
}

#ifdef KERNEL_HAS_ITER_PIPE
static size_t compute_max_pagecount(size_t size)
{
   // Compute maximal number of pages (in the pipe) that need to be present at once.
   // We don't know the page-relative offset from which max_size bytes will be reserved.
   // Assume the worst case.
   size_t max_offset = PAGE_SIZE - 1;

   size_t max_pages = (max_offset + size + PAGE_SIZE - 1) / PAGE_SIZE;

   return max_pages;
}


static void beegfs_readsink_reserve_pipe(BeeGFS_ReadSink *rs, struct iov_iter *iter, size_t size)
{
   size_t max_pages;

   // struct should be zeroed
   BUG_ON(rs->npages != 0);
   BUG_ON(rs->pages != 0);
   BUG_ON(rs->bvecs != 0);

   // should we disallow size > iter count?
   size = min_t(size_t, size, iov_iter_count(iter));
   max_pages = compute_max_pagecount(size);

   // Could be kmalloc() instead of kzalloc(), but the iov_iter_get_pages() API
   // gives back a byte count which makes it hard to detect initialization bugs
   // related to the page pointers.
   rs->pages = kzalloc(max_pages * sizeof *rs->pages, GFP_NOFS);
   if (! rs->pages)
      return;

   rs->bvecs = kmalloc(max_pages * sizeof *rs->bvecs, GFP_NOFS);
   if (! rs->bvecs)
      return;

   {
      struct bio_vec *const bvecs = rs->bvecs;
      struct page **const pages = rs->pages;

      long unsigned start;
      ssize_t gpr;

      size_t view_size = 0;

      #ifdef KERNEL_HAS_IOV_ITER_GET_PAGES2

      struct iov_iter copyIter = *iter; //Copying the iterator because iov_iter_get_pages2()
                                        //also performs the auto-advance of the iterator and
                                        //we don't want auto-advancement because in the end
                                        //of the while loop of the FhgfsOpsRemoting_readfileVec()
                                        //doing the same thing.
      gpr = iov_iter_get_pages2(&copyIter, pages, size, max_pages, &start);

      #else

      gpr = iov_iter_get_pages(iter, pages, size, max_pages, &start);

      #endif

      if (gpr < 0)
      {
         // indicate error?
         // probably not necessary. The sanitized_iter field will be initialized with count 0.
      }
      else if (gpr > 0)
      {
         size_t bvs_size = 0;
         size_t np = 0;

         view_size = gpr;

         for (np = 0; bvs_size < view_size; np++)
         {
            long unsigned offset = start;
            long unsigned len = min_t(size_t, view_size - bvs_size, PAGE_SIZE - start);

            BUG_ON(np >= max_pages);
            BUG_ON(! pages[np]);

            bvs_size += len;
            start = 0;

            bvecs[np] = (struct bio_vec) {
               .bv_page = pages[np],
               .bv_offset = offset,
               .bv_len = len,
            };
         }

         // make sure we're using all the pages that iov_iter_get_pages() gave us.
         //BUG_ON(np < max_pages && pages[np]);
         WARN_ON(np < max_pages && pages[np]);

         rs->npages = np;
      }

      BEEGFS_IOV_ITER_BVEC(&rs->sanitized_iter, READ, bvecs, rs->npages, view_size);
   }
}
#endif

void beegfs_readsink_reserve(BeeGFS_ReadSink *rs, struct iov_iter *iter, size_t size)
{
#ifdef KERNEL_HAS_ITER_PIPE
   if (iov_iter_type(iter) == ITER_PIPE)
      beegfs_readsink_reserve_pipe(rs, iter, size);
   else
      beegfs_readsink_reserve_no_pipe(rs, iter, size);
#else
      beegfs_readsink_reserve_no_pipe(rs, iter, size);
#endif
}

void beegfs_readsink_release(BeeGFS_ReadSink *rs)
{
   int npages = rs->npages;
   struct page **pages = rs->pages;

   for (int i = 0; i < npages; i++)
   {
      put_page(pages[i]);
      pages[i] = NULL;  // avoid this write?
   }

   kfree(rs->pages);
   kfree(rs->bvecs);

   memset(rs, 0, sizeof *rs);
}
