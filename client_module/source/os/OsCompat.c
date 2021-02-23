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


#if defined(KERNEL_HAS_SB_BDI) && !defined(KERNEL_HAS_BDI_SETUP_AND_REGISTER) && \
    !defined(KERNEL_HAS_SUPER_SETUP_BDI_NAME)
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

#ifndef KERNEL_HAS_HAVE_SUBMOUNTS
/**
 * enum d_walk_ret - action to talke during tree walk
 * @D_WALK_CONTINUE:	contrinue walk
 * @D_WALK_QUIT:	quit walk
 * @D_WALK_NORETRY:	quit when retry is needed
 * @D_WALK_SKIP:	skip this dentry and its children
 */
enum d_walk_ret {
	D_WALK_CONTINUE,
	D_WALK_QUIT,
	D_WALK_NORETRY,
	D_WALK_SKIP,
};

/*
 * Search for at least 1 mount point in the dentry's subdirs.
 * We descend to the next level whenever the d_subdirs
 * list is non-empty and continue searching.
 */

static enum d_walk_ret check_mount(void *data, struct dentry *dentry)
{
	int *ret = data;
	if (d_mountpoint(dentry)) {
		*ret = 1;
		return D_WALK_QUIT;
	}
	return D_WALK_CONTINUE;
}

/**
 * d_walk - walk the dentry tree
 * @parent:	start of walk
 * @data:	data passed to @enter() and @finish()
 * @enter:	callback when first entering the dentry
 * @finish:	callback when successfully finished the walk
 *
 * The @enter() and @finish() callbacks are called with d_lock held.
 */
static void d_walk(struct dentry *parent, void *data,
		   enum d_walk_ret (*enter)(void *, struct dentry *),
		   void (*finish)(void *))
{
	struct dentry *this_parent;
	struct list_head *next;
	unsigned seq = 0;
	enum d_walk_ret ret;
	bool retry = true;

again:
	read_seqbegin_or_lock(&rename_lock, &seq);
	this_parent = parent;
	spin_lock(&this_parent->d_lock);

	ret = enter(data, this_parent);
	switch (ret) {
	case D_WALK_CONTINUE:
		break;
	case D_WALK_QUIT:
	case D_WALK_SKIP:
		goto out_unlock;
	case D_WALK_NORETRY:
		retry = false;
		break;
	}
repeat:
	next = this_parent->d_subdirs.next;
resume:
	while (next != &this_parent->d_subdirs) {
		struct list_head *tmp = next;
		struct dentry *dentry = list_entry(tmp, struct dentry, d_child);
		next = tmp->next;

		if (unlikely(dentry->d_flags & DCACHE_DENTRY_CURSOR))
			continue;

		spin_lock_nested(&dentry->d_lock, DENTRY_D_LOCK_NESTED);

		ret = enter(data, dentry);
		switch (ret) {
		case D_WALK_CONTINUE:
			break;
		case D_WALK_QUIT:
			spin_unlock(&dentry->d_lock);
			goto out_unlock;
		case D_WALK_NORETRY:
			retry = false;
			break;
		case D_WALK_SKIP:
			spin_unlock(&dentry->d_lock);
			continue;
		}

		if (!list_empty(&dentry->d_subdirs)) {
			spin_unlock(&this_parent->d_lock);
#if defined(KERNEL_SPIN_RELEASE_HAS_3_ARGUMENTS)
			spin_release(&dentry->d_lock.dep_map, 1, _RET_IP_);
#else
			spin_release(&dentry->d_lock.dep_map, _RET_IP_);
#endif
			this_parent = dentry;
			spin_acquire(&this_parent->d_lock.dep_map, 0, 1, _RET_IP_);
			goto repeat;
		}
		spin_unlock(&dentry->d_lock);
	}
	/*
	 * All done at this level ... ascend and resume the search.
	 */
	rcu_read_lock();
ascend:
	if (this_parent != parent) {
		struct dentry *child = this_parent;
		this_parent = child->d_parent;

		spin_unlock(&child->d_lock);
		spin_lock(&this_parent->d_lock);

		/* might go back up the wrong parent if we have had a rename. */
		if (need_seqretry(&rename_lock, seq))
			goto rename_retry;
		/* go into the first sibling still alive */
		do {
			next = child->d_child.next;
			if (next == &this_parent->d_subdirs)
				goto ascend;
			child = list_entry(next, struct dentry, d_child);
		} while (unlikely(child->d_flags & DCACHE_DENTRY_KILLED));
		rcu_read_unlock();
		goto resume;
	}
	if (need_seqretry(&rename_lock, seq))
		goto rename_retry;
	rcu_read_unlock();
	if (finish)
		finish(data);

out_unlock:
	spin_unlock(&this_parent->d_lock);
	done_seqretry(&rename_lock, seq);
	return;

rename_retry:
	spin_unlock(&this_parent->d_lock);
	rcu_read_unlock();
	BUG_ON(seq & 1);
	if (!retry)
		return;
	seq = 1;
	goto again;
}

/**
 * have_submounts - check for mounts over a dentry
 * @parent: dentry to check.
 *
 * Return true if the parent or its subdirectories contain
 * a mount point
 */
int have_submounts(struct dentry *parent)
{
	int ret = 0;

	d_walk(parent, &ret, check_mount, NULL);

	return ret;
}
#endif
