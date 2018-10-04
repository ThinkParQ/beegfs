#include "AdjustChunkPermissionsWork.h"
#include <common/net/message/fsck/AdjustChunkPermissionsMsg.h>
#include <common/net/message/fsck/AdjustChunkPermissionsRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <toolkit/FsckException.h>
#include <program/Program.h>

AdjustChunkPermissionsWork::AdjustChunkPermissionsWork(Node& node, SynchronizedCounter* counter,
   AtomicUInt64* fileCount, AtomicUInt64* errorCount)
    : log("AdjustChunkPermissionsWork"),
      node(node),
      counter(counter),
      fileCount(fileCount),
      errorCount(errorCount)
{
}

AdjustChunkPermissionsWork::~AdjustChunkPermissionsWork()
{
}

void AdjustChunkPermissionsWork::process(char* bufIn, unsigned bufInLen, char* bufOut,
   unsigned bufOutLen)
{
   log.log(4, "Processing AdjustChunkPermissionsWork");

   try
   {
      doWork(false);
      doWork(true);
      // work package finished => increment counter
      this->counter->incCount();
   }
   catch (std::exception &e)
   {
      // exception thrown, but work package is finished => increment counter
      this->counter->incCount();

      // after incrementing counter, re-throw exception
      throw;
   }

   log.log(4, "Processed AdjustChunkPermissionsWork");
}

void AdjustChunkPermissionsWork::doWork(bool isBuddyMirrored)
{
   for ( unsigned firstLevelhashDirNum = 0;
      firstLevelhashDirNum <= META_DENTRIES_LEVEL1_SUBDIR_NUM - 1; firstLevelhashDirNum++ )
   {
      for ( unsigned secondLevelhashDirNum = 0;
         secondLevelhashDirNum < META_DENTRIES_LEVEL2_SUBDIR_NUM; secondLevelhashDirNum++ )
      {
         unsigned hashDirNum = StorageTk::mergeHashDirs(firstLevelhashDirNum,
            secondLevelhashDirNum);

         int64_t hashDirOffset = 0;
         int64_t contDirOffset = 0;
         std::string currentContDirID;
         unsigned resultCount = 0;

         do
         {
            AdjustChunkPermissionsMsg adjustChunkPermissionsMsg(hashDirNum, currentContDirID,
               ADJUST_AT_ONCE, hashDirOffset, contDirOffset, isBuddyMirrored);

            const auto respMsg = MessagingTk::requestResponse(node, adjustChunkPermissionsMsg,
               NETMSGTYPE_AdjustChunkPermissionsResp);

            if (respMsg)
            {
               auto* adjustChunkPermissionsRespMsg = (AdjustChunkPermissionsRespMsg*) respMsg.get();

               // set new parameters
               currentContDirID = adjustChunkPermissionsRespMsg->getCurrentContDirID();
               hashDirOffset = adjustChunkPermissionsRespMsg->getNewHashDirOffset();
               contDirOffset = adjustChunkPermissionsRespMsg->getNewContDirOffset();
               resultCount = adjustChunkPermissionsRespMsg->getCount();

               this->fileCount->increase(resultCount);

               if (adjustChunkPermissionsRespMsg->getErrorCount() > 0)
                  this->errorCount->increase(adjustChunkPermissionsRespMsg->getErrorCount());
            }
            else
            {
               throw FsckException("Communication error occured with node " + node.getID());
            }

            // if any of the worker threads threw an exception, we should stop now!
            if ( Program::getApp()->getSelfTerminate() )
               return;

         } while ( resultCount > 0 );
      }
   }
}
