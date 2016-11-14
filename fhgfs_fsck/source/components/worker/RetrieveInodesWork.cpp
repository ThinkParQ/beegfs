#include "RetrieveInodesWork.h"
#include <common/net/message/fsck/RetrieveInodesMsg.h>
#include <common/net/message/fsck/RetrieveInodesRespMsg.h>
#include <common/toolkit/MetaStorageTk.h>
#include <common/toolkit/MessagingTk.h>
#include <database/FsckDBException.h>
#include <toolkit/FsckException.h>

#include <program/Program.h>

#include <set>

RetrieveInodesWork::RetrieveInodesWork(FsckDB* db, Node& node, SynchronizedCounter* counter,
      AtomicUInt64& errors, unsigned hashDirStart, unsigned hashDirEnd,
      AtomicUInt64* numFileInodesFound, AtomicUInt64* numDirInodesFound,
      std::set<FsckTargetID>& usedTargets) :
   Work(),
   log("RetrieveInodesWork"), node(node), counter(counter), errors(&errors),
   usedTargets(&usedTargets), hashDirStart(hashDirStart), hashDirEnd(hashDirEnd),
   numFileInodesFound(numFileInodesFound), numDirInodesFound(numDirInodesFound),
   files(db->getFileInodesTable()), filesHandle(files->newBulkHandle()),
   dirs(db->getDirInodesTable()), dirsHandle(dirs->newBulkHandle())
{
}

void RetrieveInodesWork::process(char* bufIn, unsigned bufInLen, char* bufOut,
   unsigned bufOutLen)
{
   log.log(4, "Processing RetrieveInodesWork");

   try
   {
      doWork(false);
      doWork(true);
      // flush buffers before signaling completion
      files->flush(filesHandle);
      dirs->flush(dirsHandle);
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

   log.log(4, "Processed RetrieveInodesWork");
}

void RetrieveInodesWork::doWork(bool isBuddyMirrored)
{
   for ( unsigned firstLevelhashDirNum = hashDirStart; firstLevelhashDirNum <= hashDirEnd;
      firstLevelhashDirNum++ )
   {
      for ( unsigned secondLevelhashDirNum = 0;
         secondLevelhashDirNum < META_DENTRIES_LEVEL2_SUBDIR_NUM; secondLevelhashDirNum++ )
      {
         unsigned hashDirNum = StorageTk::mergeHashDirs(firstLevelhashDirNum,
            secondLevelhashDirNum);

         int64_t lastOffset = 0;
         size_t fileInodeCount;
         size_t dirInodeCount;

         do
         {
            bool commRes;
            char *respBuf = NULL;
            NetMessage *respMsg = NULL;

            RetrieveInodesMsg retrieveInodesMsg(hashDirNum, lastOffset,
               RETRIEVE_INODES_PACKET_SIZE, isBuddyMirrored);

            commRes = MessagingTk::requestResponse(node, &retrieveInodesMsg,
               NETMSGTYPE_RetrieveInodesResp, &respBuf, &respMsg);
            if ( commRes )
            {
               RetrieveInodesRespMsg* retrieveInodesRespMsg = (RetrieveInodesRespMsg*) respMsg;

               // set new parameters
               lastOffset = retrieveInodesRespMsg->getLastOffset();

               // parse all file inodes
               FsckFileInodeList& fileInodes = retrieveInodesRespMsg->getFileInodes();

               // check inode entry IDs
               for (auto it = fileInodes.begin(); it != fileInodes.end(); )
               {
                  if (db::EntryID::tryFromStr(it->getID()).first
                        && db::EntryID::tryFromStr(it->getParentDirID()).first
                        && (!it->getPathInfo()->hasOrigFeature()
                              || db::EntryID::tryFromStr(
                                    it->getPathInfo()->getOrigParentEntryID()).first))
                  {
                     ++it;
                     continue;
                  }

                  LOG(ERR, "Found inode with invalid entry IDs.",
                        as("node", it->getSaveNodeID()),
                        as("isBuddyMirrored", it->getIsBuddyMirrored()),
                        as("entryID", it->getID()),
                        as("parentEntryID", it->getParentDirID()),
                        as("origParent", it->getPathInfo()->getOrigParentEntryID()));

                  ++it;
                  errors->increase();
                  fileInodes.erase(std::prev(it));
               }

               // add targetIDs
               for (auto iter = fileInodes.begin(); iter != fileInodes.end(); iter++)
               {
                  FsckTargetIDType fsckTargetIDType;

                  if (iter->getStripePatternType() == FsckStripePatternType_BUDDYMIRROR)
                     fsckTargetIDType = FsckTargetIDType_BUDDYGROUP;
                  else
                     fsckTargetIDType = FsckTargetIDType_TARGET;

                  for (auto targetsIter = iter->getStripeTargets().begin();
                     targetsIter != iter->getStripeTargets().end(); targetsIter++)
                  {
                     this->usedTargets->insert(FsckTargetID(*targetsIter, fsckTargetIDType) );
                  }
               }

               // parse all directory inodes
               FsckDirInodeList& dirInodes = retrieveInodesRespMsg->getDirInodes();

               // check inode entry IDs
               for (auto it = dirInodes.begin(); it != dirInodes.end(); )
               {
                  if (db::EntryID::tryFromStr(it->getID()).first
                        && db::EntryID::tryFromStr(it->getParentDirID()).first)
                  {
                     ++it;
                     continue;
                  }

                  LOG(ERR, "Found inode with invalid entry IDs.",
                        as("node", it->getSaveNodeID()),
                        as("isBuddyMirrored", it->getIsBuddyMirrored()),
                        as("entryID", it->getID()),
                        as("parentEntryID", it->getParentDirID()));

                  ++it;
                  errors->increase();
                  dirInodes.erase(std::prev(it));
               }

               fileInodeCount = fileInodes.size();
               dirInodeCount = dirInodes.size();

               this->files->insert(fileInodes, this->filesHandle);
               this->dirs->insert(dirInodes, this->dirsHandle);

               numFileInodesFound->increase(fileInodeCount);
               numDirInodesFound->increase(dirInodeCount);

               SAFE_FREE(respBuf);
               SAFE_DELETE(respMsg);
            }
            else
            {
               SAFE_FREE(respBuf);
               SAFE_DELETE(respMsg);
               throw FsckException("Communication error occured with node " + node.getID());
            }

            // if any of the worker threads threw an exception, we should stop now!
            if ( Program::getApp()->getShallAbort() )
               return;

         } while ( (fileInodeCount + dirInodeCount) > 0 );
      }
   }
}
