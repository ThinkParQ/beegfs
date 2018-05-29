#include <app/App.h>
#include <filesystem/FhgfsOpsHelper.h>
#include <common/toolkit/Time.h>
//#include <filesystem/FhgfsInode.h>
#include "Flusher.h"


void Flusher_init(Flusher* this, App* app)
{
   // call super constructor
   Thread_init( (Thread*)this, BEEGFS_THREAD_NAME_PREFIX_STR "Flusher", __Flusher_run);

   this->app = app;
}

struct Flusher* Flusher_construct(App* app)
{
   struct Flusher* this = (Flusher*)os_kmalloc(sizeof(*this) );

   Flusher_init(this, app);

   return this;
}

void Flusher_uninit(Flusher* this)
{
   Thread_uninit( (Thread*)this);
}

void Flusher_destruct(Flusher* this)
{
   Flusher_uninit(this);

   kfree(this);
}

void _Flusher_requestLoop(Flusher* this)
{
   int sleepTimeMS = 5*1000;

   Thread* thisThread = (Thread*)this;

   while(!_Thread_waitForSelfTerminateOrder(thisThread, sleepTimeMS) )
   {
      __Flusher_flushBuffers(this);
   }

}


void __Flusher_run(Thread* this)
{
   Flusher* thisCast = (Flusher*)this;

   const char* logContext = "Flusher (run)";
   Logger* log = App_getLogger(thisCast->app);


   _Flusher_requestLoop(thisCast);

   Logger_log(log, 4, logContext, "Component stopped.");
}


void __Flusher_flushBuffers(Flusher* this)
{
   const char* logContext = "flushBuffers (async)";

   InodeRefStore* refStore = App_getInodeRefStore(this->app);
   Thread* thisThread = (Thread*)this;

   struct inode* inode = InodeRefStore_getAndRemoveFirstInode(refStore);

   while(inode)
   {
      FhgfsInode* fhgfsInode = BEEGFS_INODE(inode);

      FhgfsOpsErr flushRes = FhgfsOpsHelper_flushCacheNoWait(this->app, fhgfsInode, false);

      if(flushRes == FhgfsOpsErr_SUCCESS)
      { // flush succeeded => drop inode reference
         iput(inode);
      }
      else
      if(flushRes == FhgfsOpsErr_INUSE)
      { // file wasn't flushed, because the lock is busy right now => re-add and continue with next
         InodeRefStore_addOrPutInode(refStore, inode);
      }
      else
      if( (flushRes != FhgfsOpsErr_COMMUNICATION) || !FhgfsInode_getIsFileOpen(fhgfsInode) )
      { /* unrecoverable error and file is no longer open (so there is no chance that the user app
           can see an error code) so we have to discard the buffer to avoid retrying infintely
           on this inode */

         Logger* log = App_getLogger(this->app);

         FhgfsOpsErr finalFlushRes = FhgfsOpsHelper_flushCache(
            this->app, fhgfsInode, true);

         if(finalFlushRes != FhgfsOpsErr_SUCCESS)
         { // final flush attempt failed => notify user
            Logger_logFormatted(log, Log_DEBUG, logContext,
               "Discarded file buffer due to unrecoverable error on closed file: %s",
               FhgfsOpsErr_toErrString(finalFlushRes) );
         }

         iput(inode);
      }
      else
      { // comm error (or unrecoverable error, but file still open); flush failed => re-add inode

         /* note: decreasing ref count if inode exists in store is important in addOrPutInode(),
          * because we might race with a user app, e.g.:
          * 1) flusher gets a comm error and flusher thread sleeps before calling addOrPutInode()
          * 2) user app runs, flushes successfully, creates new cache buf and adds it to store
          * 3) flusher wakes up and calls addOrPutInode() */

         InodeRefStore_addOrPutInode(refStore, inode);
      }

      // check if user wants to unmount
      if(Thread_getSelfTerminate(thisThread) )
         break;

      // proceed to next inode
      /* note: it doesn't matter that the inode may no longer be valid here (after we dropped the
         reference), because we're not accessing the inode in the InodeRefStore methods. */
      inode = InodeRefStore_getAndRemoveNextInode(refStore, inode);
   }

}
