#include <app/log/Logger.h>
#include <app/config/Config.h>
#include <app/App.h>
#include <filesystem/FhgfsOpsPages.h>

#include "FhgfsChunkPageVec.h"

/**
 * Iterate over (remaining) pages on write and handle writeRes (might be an error)
 *
 * @param writeRes  negative linux error code on error
 */
void FhgfsChunkPageVec_iterateAllHandleWritePages(FhgfsChunkPageVec* this, int writeRes)
{
   while(1)
   {
      FhgfsPage* fhgfsPage = FhgfsChunkPageVec_iterateGetNextPage(this);
      if (!fhgfsPage)
         break;

      FhgfsOpsPages_endWritePage(fhgfsPage->page, writeRes, this->inode);
   }
}


/**
 * Unmap, unlock and release all pages in the page list vector
 */
void FhgfsChunkPageVec_iterateAllHandleReadErr(FhgfsChunkPageVec* this)
{
   FhgfsPage* fhgfsPage;

   while (1)
   {
      fhgfsPage = FhgfsChunkPageVec_iterateGetNextPage(this);
      if (!fhgfsPage)
         break;

      FhgfsPage_unmapUnlockReleaseFhgfsPage(fhgfsPage);
   }
}


