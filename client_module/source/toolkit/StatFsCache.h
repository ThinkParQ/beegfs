#ifndef STATFSCACHE_H_
#define STATFSCACHE_H_

#include <common/storage/StorageErrors.h>
#include <common/threading/RWLock.h>
#include <common/Common.h>
#include <os/OsDeps.h>


struct App; // forward declaration

struct StatFsCache;
typedef struct StatFsCache StatFsCache;


static inline void StatFsCache_init(StatFsCache* this, App* app);
static inline StatFsCache* StatFsCache_construct(App* app);
static inline void StatFsCache_destruct(StatFsCache* this);

FhgfsOpsErr StatFsCache_getFreeSpace(StatFsCache* this, int64_t* outSizeTotal,
   int64_t* outSizeFree);


/**
 * This class provides a cache for free space information to avoid querying all storage targets
 * for each statfs() syscall.
 */
struct StatFsCache
{
   App* app;

   RWLock rwlock;

   Time lastUpdateTime; // time when cache values were last updated

   int64_t cachedSizeTotal; // in bytes
   int64_t cachedSizeFree; // in bytes
};


void StatFsCache_init(StatFsCache* this, App* app)
{
   this->app = app;

   RWLock_init(&this->rwlock);

   Time_initZero(&this->lastUpdateTime);
}

struct StatFsCache* StatFsCache_construct(App* app)
{
   struct StatFsCache* this = (StatFsCache*)os_kmalloc(sizeof(*this) );

   if(likely(this) )
      StatFsCache_init(this, app);

   return this;
}

void StatFsCache_destruct(StatFsCache* this)
{
   kfree(this);
}



#endif /* STATFSCACHE_H_ */
