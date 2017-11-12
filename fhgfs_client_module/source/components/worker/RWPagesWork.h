#ifndef RWPAGESWORK_H_
#define RWPAGESWORK_H_

#include <common/Common.h>
#include <common/toolkit/SynchronizedCounter.h>
#include <toolkit/FhgfsPage.h>
#include <toolkit/FhgfsChunkPageVec.h>
#include <net/filesystem/RemotingIOInfo.h>
#include <filesystem/FhgfsInode.h>
#include <filesystem/FhgfsOpsFile.h>
#include <filesystem/FsFileInfo.h>
#include <common/threading/AtomicInt.h>
#include <net/filesystem/FhgfsOpsRemoting.h>

#include <linux/fs.h>


struct RWPagesWork;
typedef struct RWPagesWork RWPagesWork;

extern bool RWPagesWork_createQueue(App* app, FhgfsChunkPageVec* pageVec, struct inode* inode,
   Fhgfs_RWType rwType);

bool RWPagesWork_init(RWPagesWork* this, App* app, struct inode* inode,
   FhgfsChunkPageVec *pageVec, Fhgfs_RWType rwType);
static inline RWPagesWork* RWPagesWork_construct(App* app, struct inode* inode,
   FhgfsChunkPageVec *pageVec, Fhgfs_RWType rwType);
static inline void RWPagesWork_uninit(RWPagesWork* this);
static inline void RWPagesWork_destruct(RWPagesWork* this);

extern bool RWPagesWork_initworkQueue(void);
extern void RWPagesWork_destroyWorkQueue(void);
extern void RWPagesWork_flushWorkQueue(void);


// virtual functions
extern void RWPagesWork_process(struct work_struct* work);
extern void RWPagesWork_oldProcess(void* data);

struct RWPagesWork
{
      struct work_struct kernelWork;

      App* app;

      FhgfsChunkPageVec* pageVec;

      RemotingIOInfo ioInfo;

      FileHandleType handleType;
      Fhgfs_RWType rwType;

      struct inode* inode;
};


/**
 * RWPagesWork Constructor
 */
struct RWPagesWork* RWPagesWork_construct(App* app, struct inode* inode,
   FhgfsChunkPageVec * pageVec, Fhgfs_RWType rwType)
{
   bool initRes;

   struct RWPagesWork* this = (RWPagesWork*)os_kmalloc(sizeof(*this) );
   if (unlikely(!this) )
      return NULL;

   initRes = RWPagesWork_init(this, app, inode, pageVec, rwType);

   if (unlikely(!initRes) )
   {  // uninitilize everything that already has been initialized and free this

      if (rwType == BEEGFS_RWTYPE_READ)
         FhgfsChunkPageVec_iterateAllHandleReadErr(pageVec);
      else
         FhgfsChunkPageVec_iterateAllHandleWritePages(pageVec, -EIO);

      RWPagesWork_destruct(this);
      this = NULL;
   }

   return this;
}

/**
 * Unitinialize worker data
 */
void RWPagesWork_uninit(RWPagesWork* this)
{
   FhgfsInode* fhgfsInode = BEEGFS_INODE(this->inode);

   FhgfsChunkPageVec_destroy(this->pageVec);

   FhgfsInode_releaseHandle(fhgfsInode, this->handleType, NULL);
}

void RWPagesWork_destruct(RWPagesWork* this)
{
   RWPagesWork_uninit(this);
   kfree(this);
}

#endif /* RWPAGESWORK_H_ */
