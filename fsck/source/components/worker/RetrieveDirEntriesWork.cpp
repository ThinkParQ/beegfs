#include "RetrieveDirEntriesWork.h"
#include <common/net/message/fsck/RetrieveDirEntriesMsg.h>
#include <common/net/message/fsck/RetrieveDirEntriesRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/MetaStorageTk.h>
#include <database/FsckDBException.h>
#include <toolkit/FsckException.h>

#include <program/Program.h>

#include <set>

RetrieveDirEntriesWork::RetrieveDirEntriesWork(FsckDB* db, Node& node, SynchronizedCounter* counter,
      AtomicUInt64& errors, unsigned hashDirStart, unsigned hashDirEnd,
      AtomicUInt64* numDentriesFound, AtomicUInt64* numFileInodesFound,
      std::set<FsckTargetID>& usedTargets) :
   log("RetrieveDirEntriesWork"), node(node), counter(counter), errors(&errors),
   numDentriesFound(numDentriesFound), numFileInodesFound(numFileInodesFound),
   usedTargets(&usedTargets), hashDirStart(hashDirStart), hashDirEnd(hashDirEnd),
   dentries(db->getDentryTable()), dentriesHandle(dentries->newBulkHandle()),
   files(db->getFileInodesTable()), filesHandle(files->newBulkHandle()),
   contDirs(db->getContDirsTable()), contDirsHandle(contDirs->newBulkHandle())
{
}

void RetrieveDirEntriesWork::process(char* bufIn, unsigned bufInLen, char* bufOut,
   unsigned bufOutLen)
{
   log.log(4, "Processing RetrieveDirEntriesWork");

   try
   {
      doWork(false);
      doWork(true);
      // flush buffers before signaling completion
      dentries->flush(dentriesHandle);
      files->flush(filesHandle);
      contDirs->flush(contDirsHandle);
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

   log.log(4, "Processed RetrieveDirEntriesWork");
}

void RetrieveDirEntriesWork::doWork(bool isBuddyMirrored)
{
   for ( unsigned firstLevelhashDirNum = hashDirStart; firstLevelhashDirNum <= hashDirEnd;
      firstLevelhashDirNum++ )
   {
      for ( unsigned secondLevelhashDirNum = 0;
         secondLevelhashDirNum < META_DENTRIES_LEVEL2_SUBDIR_NUM; secondLevelhashDirNum++ )
      {
         unsigned hashDirNum = StorageTk::mergeHashDirs(firstLevelhashDirNum,
            secondLevelhashDirNum);

         int64_t hashDirOffset = 0;
         int64_t contDirOffset = 0;
         std::string currentContDirID;
         int resultCount = 0;

         do
         {
            RetrieveDirEntriesMsg retrieveDirEntriesMsg(hashDirNum, currentContDirID,
               RETRIEVE_DIR_ENTRIES_PACKET_SIZE, hashDirOffset, contDirOffset, isBuddyMirrored);

            const auto respMsg = MessagingTk::requestResponse(node, retrieveDirEntriesMsg,
               NETMSGTYPE_RetrieveDirEntriesResp);

            if (respMsg)
            {
               auto* retrieveDirEntriesRespMsg = (RetrieveDirEntriesRespMsg*) respMsg.get();

               // set new parameters
               currentContDirID = retrieveDirEntriesRespMsg->getCurrentContDirID();
               hashDirOffset = retrieveDirEntriesRespMsg->getNewHashDirOffset();
               contDirOffset = retrieveDirEntriesRespMsg->getNewContDirOffset();

               // parse directory entries
               FsckDirEntryList& dirEntries = retrieveDirEntriesRespMsg->getDirEntries();
               // this is the actual result count we are interested in, because if no dirEntries
               // were read, there is nothing left on the server

               resultCount = dirEntries.size();

               // check dentry entry IDs
               for (auto it = dirEntries.begin(); it != dirEntries.end(); )
               {
                  if (db::EntryID::tryFromStr(it->getID()).first
                        && db::EntryID::tryFromStr(it->getParentDirID()).first)
                  {
                     ++it;
                     continue;
                  }

                  LOG(GENERAL, ERR, "Found dentry with invalid entry IDs.",
                        ("node", it->getSaveNodeID()),
                        ("isBuddyMirrored", it->getIsBuddyMirrored()),
                        ("entryID", it->getID()),
                        ("parentEntryID", it->getParentDirID()));

                  ++it;
                  errors->increase();
                  dirEntries.erase(std::prev(it));
               }

               this->dentries->insert(dirEntries, this->dentriesHandle);

               numDentriesFound->increase(resultCount);

               // parse inlined file inodes
               FsckFileInodeList& inlinedFileInodes =
                  retrieveDirEntriesRespMsg->getInlinedFileInodes();

               // check inode entry IDs
               for (auto it = inlinedFileInodes.begin(); it != inlinedFileInodes.end(); )
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

                  LOG(GENERAL, ERR, "Found inode with invalid entry IDs.",
                        ("node", it->getSaveNodeID()),
                        ("isBuddyMirrored", it->getIsBuddyMirrored()),
                        ("entryID", it->getID()),
                        ("parentEntryID", it->getParentDirID()),
                        ("origParent", it->getPathInfo()->getOrigParentEntryID()));

                  ++it;
                  errors->increase();
                  inlinedFileInodes.erase(std::prev(it));
               }

               struct ops
               {
                  static bool dentryCmp(const FsckDirEntry& a, const FsckDirEntry& b)
                  {
                     return a.getID() < b.getID();
                  }

                  static bool inodeCmp(const FsckFileInode& a, const FsckFileInode& b)
                  {
                     return a.getID() < b.getID();
                  }
               };

               dirEntries.sort(ops::dentryCmp);
               inlinedFileInodes.sort(ops::inodeCmp);

               this->files->insert(inlinedFileInodes, this->filesHandle);

               numFileInodesFound->increase(inlinedFileInodes.size());

               // add used targetIDs
               for ( FsckFileInodeListIter iter = inlinedFileInodes.begin();
                  iter != inlinedFileInodes.end(); iter++ )
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

               // parse all new cont. directories
               FsckContDirList& contDirs = retrieveDirEntriesRespMsg->getContDirs();

               // check entry IDs
               for (auto it = contDirs.begin(); it != contDirs.end(); )
               {
                  if (db::EntryID::tryFromStr(it->getID()).first)
                  {
                     ++it;
                     continue;
                  }

                  LOG(GENERAL, ERR, "Found content directory with invalid entry ID.",
                        ("node", it->getSaveNodeID()),
                        ("isBuddyMirrored", it->getIsBuddyMirrored()),
                        ("entryID", it->getID()));

                  ++it;
                  errors->increase();
                  contDirs.erase(std::prev(it));
               }

               this->contDirs->insert(contDirs, this->contDirsHandle);
            }
            else
            {
               throw FsckException("Communication error occured with node " + node.getID());
            }

            // if any of the worker threads threw an exception, we should stop now!
            if ( Program::getApp()->getShallAbort() )
               return;

         } while ( resultCount > 0 );
      }
   }
}
