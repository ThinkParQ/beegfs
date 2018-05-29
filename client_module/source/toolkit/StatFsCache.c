#include <app/App.h>
#include <net/filesystem/FhgfsOpsRemoting.h>
#include "StatFsCache.h"


/**
 * If caching is enabled and cache is not expired, this will return the cached values. Otherwise
 * FhgfsOpsRemoting_statStoragePath() will be called for fresh values.
 *
 * @param outSizeTotal in bytes.
 * @param outSizeFree in bytes.
 * @return if remoting fails, the error code from FhgfsOpsRemoting_statStoragePath() will be
 * returned.
 */
FhgfsOpsErr StatFsCache_getFreeSpace(StatFsCache* this, int64_t* outSizeTotal,
  int64_t* outSizeFree)
{
   Config* cfg = App_getConfig(this->app);
   unsigned tuneStatFsCacheSecs = Config_getTuneStatFsCacheSecs(cfg); // zero means disable caching

   if(tuneStatFsCacheSecs)
   { // caching enabled, test if cache values expired

      RWLock_readLock(&this->rwlock); // L O C K (read)

      if(!Time_getIsZero(&this->lastUpdateTime) && // zero time means cache values are uninitialized
         (Time_elapsedMS(&this->lastUpdateTime) <= (tuneStatFsCacheSecs*1000) ) ) // *1000 for MS
      { // cache values are still valid
         *outSizeTotal = this->cachedSizeTotal;
         *outSizeFree = this->cachedSizeFree;

         RWLock_readUnlock(&this->rwlock); // U N L O C K (read)

         return FhgfsOpsErr_SUCCESS;
      }

      // cache values not valid

      RWLock_readUnlock(&this->rwlock); // U N L O C K (read)
   }

   // get fresh values from servers

   {
      FhgfsOpsErr retVal = FhgfsOpsRemoting_statStoragePath(this->app, true,
         outSizeTotal, outSizeFree);

      if(retVal != FhgfsOpsErr_SUCCESS)
      {
         *outSizeTotal = 0;
         *outSizeFree = 0;

         return retVal;
      }
   }

   // update cached values
   /* note: as we don't hold a write-lock during remoting (to allow parallelism), it's possible that
      multiple threads are racing with different values on the update below, but that's uncritical
      for free space info */

   if(tuneStatFsCacheSecs)
   {
      RWLock_writeLock(&this->rwlock); // L O C K (write)

      this->cachedSizeTotal = *outSizeTotal;
      this->cachedSizeFree = *outSizeFree;

      Time_setToNow(&this->lastUpdateTime);

      RWLock_writeUnlock(&this->rwlock); // U N L O C K (write)
   }


   return FhgfsOpsErr_SUCCESS;
}
