/*
 * Compatibility functions for older Linux versions
 */

#include <linux/mm.h> // for old sles10 kernels, which forgot to include it in backing-dev.h
#include <linux/backing-dev.h>
#include <linux/pagemap.h>
#include <linux/uio.h>
#include <linux/writeback.h>

#include <app/App.h>
#include <app/log/Logger.h>
#include <common/Common.h>
#include <filesystem/FhgfsOpsSuper.h>

#ifndef KERNEL_HAS_MEMDUP_USER
   /**
    * memdup_user - duplicate memory region from user space
    *
    * @src: source address in user space
    * @len: number of bytes to copy
    *
    * Returns an ERR_PTR() on failure.
    */
   void *memdup_user(const void __user *src, size_t len)
   {
      void *p;

      /*
       * Always use GFP_KERNEL, since copy_from_user() can sleep and
       * cause pagefault, which makes it pointless to use GFP_NOFS
       * or GFP_ATOMIC.
       */
      p = kmalloc(len, GFP_KERNEL);
      if (!p)
         return ERR_PTR(-ENOMEM);

      if (copy_from_user(p, src, len)) {
         kfree(p);
         return ERR_PTR(-EFAULT);
      }

      return p;
   }
#endif // memdup_user, LINUX_VERSION_CODE < KERNEL_VERSION(2,6,30)


#if defined(KERNEL_HAS_SB_BDI) && !defined(KERNEL_HAS_BDI_SETUP_AND_REGISTER)
   /*
    * For use from filesystems to quickly init and register a bdi associated
    * with dirty writeback
    */
   int bdi_setup_and_register(struct backing_dev_info *bdi, char *name,
               unsigned int cap)
   {
      static atomic_long_t fhgfs_bdiSeq = ATOMIC_LONG_INIT(0);
      char tmp[32];
      int err;

      bdi->name = name;
      bdi->capabilities = cap;
      err = bdi_init(bdi);
      if (err)
         return err;

      sprintf(tmp, "%.28s%s", name, "-%d");
      err = bdi_register(bdi, NULL, tmp, atomic_long_inc_return(&fhgfs_bdiSeq));
      if (err) {
         bdi_destroy(bdi);
         return err;
      }

      return 0;
   }
#endif



/* NOTE: We can't do a feature detection for find_get_pages_tag(), as 
 *       this function is in all headers of all supported kernel versions.
 *       However, it is only _exported_ since 2.6.22 and also only 
 *       exported in RHEL >=5.10. */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
   /**
    * find_get_pages_tag - find and return pages that match @tag
    * @mapping:   the address_space to search
    * @index:  the starting page index
    * @tag: the tag index
    * @nr_pages:  the maximum number of pages
    * @pages:  where the resulting pages are placed
    *
    * Like find_get_pages, except we only return pages which are tagged with
    * @tag.   We update @index to index the next page for the traversal.
    */
   unsigned find_get_pages_tag(struct address_space *mapping, pgoff_t *index,
            int tag, unsigned int nr_pages, struct page **pages)
   {
      unsigned int i;
      unsigned int ret;

      read_lock_irq(&mapping->tree_lock);
      ret = radix_tree_gang_lookup_tag(&mapping->page_tree,
               (void **)pages, *index, nr_pages, tag);
      for (i = 0; i < ret; i++)
         page_cache_get(pages[i]);
      if (ret)
         *index = pages[ret - 1]->index + 1;
      read_unlock_irq(&mapping->tree_lock);
      return ret;
   }
#endif // find_get_pages_tag() for <2.6.22


#ifndef KERNEL_HAS_D_MAKE_ROOT

/**
 * This is the former d_alloc_root with an additional iput on error.
 */
struct dentry *d_make_root(struct inode *root_inode)
{
   struct dentry* allocRes = d_alloc_root(root_inode);
   if(!allocRes)
      iput(root_inode);

   return allocRes;
}
#endif

#ifndef KERNEL_HAS_D_MATERIALISE_UNIQUE
/**
 * d_materialise_unique() was merged into d_splice_alias() in linux-3.19
 */
struct dentry* d_materialise_unique(struct dentry *dentry, struct inode *inode)
{
   return d_splice_alias(inode, dentry);
}
#endif // KERNEL_HAS_D_MATERIALISE_UNIQUE

/**
 * Note: Call this once during module init (and remember to call kmem_cache_destroy() )
 */
#if defined(KERNEL_HAS_KMEMCACHE_CACHE_FLAGS_CTOR)
struct kmem_cache* OsCompat_initKmemCache(const char* cacheName, size_t cacheSize,
   void initFuncPtr(void* initObj, struct kmem_cache* cache, unsigned long flags) )
#elif defined(KERNEL_HAS_KMEMCACHE_CACHE_CTOR)
struct kmem_cache* OsCompat_initKmemCache(const char* cacheName, size_t cacheSize,
   void initFuncPtr(struct kmem_cache* cache, void* initObj) )
#else
struct kmem_cache* OsCompat_initKmemCache(const char* cacheName, size_t cacheSize,
   void initFuncPtr(void* initObj) )
#endif // LINUX_VERSION_CODE
{
   struct kmem_cache* cache;

   unsigned long cacheFlags = SLAB_RECLAIM_ACCOUNT | SLAB_MEM_SPREAD;

#if defined(KERNEL_HAS_KMEMCACHE_DTOR)
   cache = kmem_cache_create(cacheName, cacheSize, 0, cacheFlags, initFuncPtr, NULL);
#else
   cache = kmem_cache_create(cacheName, cacheSize, 0, cacheFlags, initFuncPtr);
#endif // LINUX_VERSION_CODE


   return cache;
}

#ifndef rbtree_postorder_for_each_entry_safe
static struct rb_node* rb_left_deepest_node(const struct rb_node* node)
{
   for (;;)
   {
      if (node->rb_left)
         node = node->rb_left;
      else
      if (node->rb_right)
         node = node->rb_right;
      else
         return (struct rb_node*) node;
   }
}

struct rb_node* rb_next_postorder(const struct rb_node* node)
{
   const struct rb_node *parent;

   if (!node)
      return NULL;

   parent = rb_parent(node);

   /* If we're sitting on node, we've already seen our children */
   if (parent && node == parent->rb_left && parent->rb_right)
   {
      /* If we are the parent's left node, go to the parent's right
       * node then all the way down to the left */
      return rb_left_deepest_node(parent->rb_right);
   }
   else
      /* Otherwise we are the parent's right node, and the parent
       * should be next */
      return (struct rb_node*) parent;
}

struct rb_node* rb_first_postorder(const struct rb_root* root)
{
   if (!root->rb_node)
      return NULL;

   return rb_left_deepest_node(root->rb_node);
}
#endif

#ifndef KERNEL_HAS_WRITE_CACHE_PAGES
int os_write_cache_pages(struct address_space* mapping, struct writeback_control* wbc,
   int (*writepage)(struct page*, struct writeback_control*, void*), void* data)
{
   struct backing_dev_info* bdi = mapping->backing_dev_info;
   int ret = 0;
   int done = 0;
   struct pagevec pvec;
   int nr_pages;
   pgoff_t index;
   pgoff_t end;		/* Inclusive */
   int scanned = 0;
   int range_whole = 0;

   if(wbc->nonblocking && bdi_write_congested(bdi) )
   {
      wbc->encountered_congestion = 1;
      return 0;
   }

   pagevec_init(&pvec, 0);
   if(wbc->range_cyclic)
   {
      index = mapping->writeback_index; /* Start from prev offset */
      end = -1;
   }
   else
   {
      index = wbc->range_start >> PAGE_CACHE_SHIFT;
      end = wbc->range_end >> PAGE_CACHE_SHIFT;
      if(wbc->range_start == 0 && wbc->range_end == LLONG_MAX)
         range_whole = 1;

      scanned = 1;
   }

retry:
   while(!done && (index <= end) &&
         (nr_pages = pagevec_lookup_tag(&pvec, mapping, &index,
                                        PAGECACHE_TAG_DIRTY,
                                        min(end - index, (pgoff_t)PAGEVEC_SIZE-1) + 1) ) )
   {
      unsigned i;

      scanned = 1;
      for(i = 0; i < nr_pages; i++)
      {
         struct page *page = pvec.pages[i];

         /*
          * At this point we hold neither mapping->tree_lock nor
          * lock on the page itself: the page may be truncated or
          * invalidated (changing page->mapping to NULL), or even
          * swizzled back from swapper_space to tmpfs file
          * mapping
          */
         lock_page(page);

         if(unlikely(page->mapping != mapping) )
         {
            unlock_page(page);
            continue;
         }

         if(!wbc->range_cyclic && page->index > end)
         {
            done = 1;
            unlock_page(page);
            continue;
         }

         if(wbc->sync_mode != WB_SYNC_NONE)
            wait_on_page_writeback(page);

         if(PageWriteback(page) || !clear_page_dirty_for_io(page) )
         {
            unlock_page(page);
            continue;
         }

         ret = (*writepage)(page, wbc, data);

         if(unlikely(ret == AOP_WRITEPAGE_ACTIVATE) )
            unlock_page(page);

         if(ret || (--(wbc->nr_to_write) <= 0) )
            done = 1;

         if(wbc->nonblocking && bdi_write_congested(bdi) )
         {
            wbc->encountered_congestion = 1;
            done = 1;
         }
      }

      pagevec_release(&pvec);
      cond_resched();
   }

   if(!scanned && !done)
   {
      /*
       * We hit the last page and there is more work to be done: wrap
       * back to the start of the file
       */
      scanned = 1;
      index = 0;
      goto retry;
   }

   if (wbc->range_cyclic || (range_whole && wbc->nr_to_write > 0) )
      mapping->writeback_index = index;

   return ret;
}
#endif

#ifdef KERNEL_HAS_GENERIC_WRITE_CHECKS_ITER
int os_generic_write_checks(struct file* filp, loff_t* offset, size_t* size, int isblk)
{
   struct iovec iov = { 0, *size };
   struct iov_iter iter;
   ssize_t checkRes;
   struct kiocb iocb;

   iov_iter_init(&iter, WRITE, &iov, 1, *size);
   init_sync_kiocb(&iocb, filp);
   iocb.ki_pos = *offset;

   checkRes = generic_write_checks(&iocb, &iter);
   if(checkRes < 0)
      return checkRes;

   *offset = iocb.ki_pos;
   *size = iter.count;

   return 0;
}
#endif