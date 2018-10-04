#include <toolkit/NoAllocBufferStore.h>
#include <common/toolkit/tree/PointerRBTree.h>
#include <common/toolkit/tree/PointerRBTreeIter.h>

#ifdef BEEGFS_DEBUG
   static int __NoAllocBufferStore_PointerComparator(const void* ptr1, const void* ptr2);
   static void __NoAllocBufferStore_debugAddTask(NoAllocBufferStore* this);
   static void __NoAllocBufferStore_debugRemoveTask(NoAllocBufferStore* this);
   static void __NoAllocBufferStore_debugCheckTask(NoAllocBufferStore* this);
#endif

/**
 * This BufferStore does not use any kind of memory allocation during its normal
 * operation. However, it does (de)allocate the requested number of buffers
 * during (de)initialization.
 */

static bool __NoAllocBufferStore_initBuffers(NoAllocBufferStore* this);


struct NoAllocBufferStore
{
   char** bufArray;

   size_t numBufs;
   size_t bufSize;

   size_t numAvailable; // number of currently available buffers in the store

   Mutex mutex;
   Condition newBufCond;

#ifdef BEEGFS_DEBUG
   RBTree pidDebugTree; // store tasks that have taken a buffer
#endif
};


#ifdef BEEGFS_DEBUG
/**
 * Return suitable values for a tree
 */
static int __NoAllocBufferStore_PointerComparator(const void* ptr1, const void* ptr2)
{
   if (ptr1 < ptr2)
      return -1;
   else if (ptr1 == ptr2)
      return 0;

   return 1;
}
#endif // BEEGFS_DEBUG

/**
 * @param numBufs number of buffers to be allocated, may be 0
 * @param bufSize size of each allocated buffer
 */
bool NoAllocBufferStore_init(NoAllocBufferStore* this, size_t numBufs, size_t bufSize)
{
   this->numAvailable = 0;
   this->numBufs = numBufs;
   this->bufSize = bufSize;

   Mutex_init(&this->mutex);
   Condition_init(&this->newBufCond);

#ifdef BEEGFS_DEBUG
   PointerRBTree_init(&this->pidDebugTree, __NoAllocBufferStore_PointerComparator);
#endif

   if(!__NoAllocBufferStore_initBuffers(this) )
   {
      Mutex_uninit(&this->mutex);
      return false;
   }

   return true;
}

#ifdef BEEGFS_DEBUG
/**
 * Add the buffer allocator into the debug tree, including the call trace. That way we can easily
 * figure out two (forbidden) calls from the same thread. For debug builds only!
 */
static void __NoAllocBufferStore_debugAddTask(NoAllocBufferStore* this)
{
   void * trace = os_saveStackTrace();
   PointerRBTree_insert(&this->pidDebugTree, (void *)(long)current->pid, trace);
}
#else
#define __NoAllocBufferStore_debugAddTask(this)
#endif // BEEGFS_DEBUG


#ifdef BEEGFS_DEBUG
/**
 * Tasks frees the buffer, so also remove it from the debug tree. For debug builds only!
 */
static void __NoAllocBufferStore_debugRemoveTask(NoAllocBufferStore* this)
{
   void * currentPID = (void *)(long)current->pid;
   RBTreeIter iter = PointerRBTree_find(&this->pidDebugTree, currentPID);
   void *trace;

   if (PointerRBTreeIter_end(&iter) == true)
      return; // Buffer was likely/hopefully added with NoAllocBufferStore_instantBuf()

   trace = PointerRBTreeIter_value(&iter);
   os_freeStackTrace(trace);

   PointerRBTree_erase(&this->pidDebugTree, currentPID);
}
#else
#define __NoAllocBufferStore_debugRemoveTask(this)
#endif

#ifdef BEEGFS_DEBUG
/**
 * Check if the debug tree already knows about the current pid and if so, print a debug message
 * to syslog to warn about the possible deadlock. NOTE: For debug builds only!
 */
static void __NoAllocBufferStore_debugCheckTask(NoAllocBufferStore* this)
{
   RBTreeIter iter = PointerRBTree_find(&this->pidDebugTree, (void *)(long)current->pid);
   if (unlikely(PointerRBTreeIter_end(&iter) == false) )
   {
      void *trace = PointerRBTreeIter_value(&iter);
      printk_fhgfs(KERN_WARNING, "Thread called BufferStore two times - might deadlock!\n");
      dump_stack();
      printk_fhgfs(KERN_WARNING, "Call trace of the previous allocation:\n");
      os_printStackTrace(trace, 4);
   }
}
#else
#define __NoAllocBufferStore_debugCheckTask(this)
#endif // BEEGFS_DEBUG


/**
 * @param numBufs number of buffers to be allocated
 * @param bufSize size of each allocated buffer
 */
struct NoAllocBufferStore* NoAllocBufferStore_construct(size_t numBufs, size_t bufSize)
{
   struct NoAllocBufferStore* this = kmalloc(sizeof(*this), GFP_NOFS);

   if(!this ||
      !NoAllocBufferStore_init(this, numBufs, bufSize) )
   {
      kfree(this);
      return NULL;
   }

   return this;
}

void NoAllocBufferStore_uninit(NoAllocBufferStore* this)
{
   // delete buffers
   size_t i;

   for(i=0; i < this->numAvailable; i++)
      vfree(this->bufArray[i]);

   // normal clean-up
   Mutex_uninit(&this->mutex);

   SAFE_KFREE(this->bufArray);

#ifdef BEEGFS_DEBUG
   PointerRBTree_uninit(&this->pidDebugTree);
#endif

}

void NoAllocBufferStore_destruct(NoAllocBufferStore* this)
{
   NoAllocBufferStore_uninit(this);

   kfree(this);
}

/**
 * Gets a buffer from the store.
 * Waits if no buffer is immediately available.
 *
 * @return a valid buffer pointer
 */
char* NoAllocBufferStore_waitForBuf(NoAllocBufferStore* this)
{
   char* buf;

   Mutex_lock(&this->mutex);

   __NoAllocBufferStore_debugCheckTask(this);

   while(!this->numAvailable)
      Condition_wait(&this->newBufCond, &this->mutex);

   buf = (this->bufArray)[this->numAvailable-1];
   (this->numAvailable)--;

   __NoAllocBufferStore_debugAddTask(this);

   Mutex_unlock(&this->mutex);

   return buf;
}

/**
 * Gets a buffer from the store.
 * Fails if no buffer is immediately available.
 *
 * @return buffer pointer if a buffer was immediately available, NULL otherwise
 */
char* NoAllocBufferStore_instantBuf(NoAllocBufferStore* this)
{
   char* buf;

   Mutex_lock(&this->mutex);

   if(!this->numAvailable)
      buf = NULL;
   else
   {
      buf = (this->bufArray)[this->numAvailable-1];
      (this->numAvailable)--;

      /* note: no _debugAddTask() here, because _instantBuf() is specifically intended to avoid
         deadlocks (so we would generate only a lot of false alarms with this). */
   }

   Mutex_unlock(&this->mutex);

   return buf;
}

/**
 * Re-add a buffer to the pool
 */
void NoAllocBufferStore_addBuf(NoAllocBufferStore* this, char* buf)
{
   if (unlikely(buf == NULL) )
   {
      BEEGFS_BUG_ON(buf == NULL, "NULL buffer detected!");
      return; // If the caller had a real buffer, it will leak now!
   }

   Mutex_lock(&this->mutex);

   Condition_signal(&this->newBufCond);

   (this->bufArray)[this->numAvailable] = buf;
   (this->numAvailable)++;

   __NoAllocBufferStore_debugRemoveTask(this);

   Mutex_unlock(&this->mutex);
}

bool __NoAllocBufferStore_initBuffers(NoAllocBufferStore* this)
{
   size_t i;

   this->bufArray = this->numBufs ? (char**)os_kzalloc(this->numBufs * sizeof(char*) ) : NULL;
   if (!this->bufArray && (this->numBufs != 0) ) // numBufs is 0 for pageIO buffers in buffered mode
      return false;

   for(i = 0; i < this->numBufs; i++)
   {
      char* buf = vmalloc(this->bufSize);
      if(!buf)
      {
         printk_fhgfs(KERN_WARNING, "NoAllocBufferStore_initBuffers: vmalloc failed to alloc "
            "%lld bytes for buffer number %lld\n", (long long)this->bufSize, (long long)i);

         goto error;
      }

      (this->bufArray)[i] = buf;
      (this->numAvailable)++;
   }

   return true;

error:
   for(i = 0; i < this->numBufs; i++)
      vfree(this->bufArray[i]);

   return false;
}

size_t NoAllocBufferStore_getNumAvailable(NoAllocBufferStore* this)
{
   size_t numAvailable;

   Mutex_lock(&this->mutex);

   numAvailable = this->numAvailable;

   Mutex_unlock(&this->mutex);

   return numAvailable;
}

size_t NoAllocBufferStore_getBufSize(NoAllocBufferStore* this)
{
   return this->bufSize;
}
