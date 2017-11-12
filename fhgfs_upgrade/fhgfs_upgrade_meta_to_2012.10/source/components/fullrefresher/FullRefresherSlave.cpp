#include <app/App.h>
#include <app/config/Config.h>
#include <common/toolkit/Time.h>
#include <common/storage/EntryInfo.h>
#include <net/msghelpers/MsgHelperStat.h>
#include <program/Program.h>
#include <storage/MetadataEx.h>
#include "FullRefresher.h"


// FIXME Sven / Bernd also need to refresh inlined inodes

FullRefresherSlave::FullRefresherSlave() throw(ComponentInitException) : PThread("Refresher")
{
   log.setContext("RefresherSlave");

   this->isRunning = false;
   this->numDirsRefreshed = 0;
   this->numFilesRefreshed = 0;
}

FullRefresherSlave::~FullRefresherSlave()
{
   // nothing to be done here
}


/**
 * This is a singleton component, which is started manually (by user command) at runtime and
 * terminates when it's done.
 * We have to make sure here that we don't get multiple instances of this thread running at the
 * same time.
 */
void FullRefresherSlave::run()
{
   setIsRunning(true);

   try
   {
      registerSignalHandler();

      updateAllMetadata();

      log.log(4, "Component stopped.");
   }
   catch(std::exception& e)
   {
      PThread::getCurrentThreadApp()->handleComponentException(e);
   }

   setIsRunning(false);
}

/**
 * Refresh/recalculate metadata of all local entries.
 */
void FullRefresherSlave::updateAllMetadata()
{
   App* app = Program::getApp();
   MetaStore* metaStore = app->getMetaStore();

   const unsigned maxEntries = 100;

   uint64_t numDirsRefreshed = 0;
   uint64_t numFilesRefreshed = 0;

   // (note: member stats counters are set to 0 in refresher master before running this thread.)

   log.log(2, "Starting full metadata refresh...");

   // FIXME Bernd / Sven: Make it work with subDirs again
   // loop: walk over all hash directories;
   for ( unsigned i = 0; i < META_INODES_LEVEL1_SUBDIR_NUM; i++ )
   {
      for ( unsigned j = 0; j < META_INODES_LEVEL2_SUBDIR_NUM; j++ )
      {
         StringList entryIDFiles;
         int64_t lastOffset = 0;

         // loop: walk over all files in the current hash directory
         do
         {
            entryIDFiles.clear();

            metaStore->getAllEntryIDFilesIncremental(i, j, lastOffset, maxEntries, &entryIDFiles,
               &lastOffset);

            // loop: walk over all files returned from the current hash dir query
            for ( StringListIter iter = entryIDFiles.begin(); iter != entryIDFiles.end(); iter++ )
            {
               /* note: it's important to check self-termination request in the inner loop to avoid
                long timeout waits when the cluster is shutting down and other servers unreachable.*/
               if ( getSelfTerminate() )
                  goto end;

               updateNotInlinedEntryByID(*iter, &numDirsRefreshed, &numFilesRefreshed);
            }

            setNumRefreshed(numDirsRefreshed, numFilesRefreshed); // update stats counters

         } while ( !entryIDFiles.empty() );

      }
   }


   end:

   log.log(2, "End of full refresh.");
}

/**
 * Refresh/recalculate metadata of an entry.
 *
 * @param entryIDFile filename of the entry metadata file
 * @inoutNumDirsRefreshed in: current num dirs refreshed, out: updated num dirs refreshed (i.e.
 * inc'ed by 1 if the entry was a dir).
 * @inoutNumFilesRefreshed in: current num files refreshed, out: updated num files refreshed (i.e.
 * inc'ed by 1 if the entry was a file).
 */
void FullRefresherSlave::updateNotInlinedEntryByID(std::string entryID,
   uint64_t* inoutNumDirsRefreshed, uint64_t* inoutNumFilesRefreshed)
{
   App* app = Program::getApp();
   MetaStore* metaStore = app->getMetaStore();

   // find out whether it is a file or dir inode and open it
   DirInode* dirInode = NULL;
   FileInode* fileInode = NULL;

   DirEntryType entryType = metaStore->referenceInode(entryID, &fileInode, &dirInode);

   // refresh the metadata for this dir or file
   // (note: we carefully check both cases here, even if the first one matched already)
   if ( (DirEntryType_ISDIR(entryType)) && (dirInode) )
   {
      // entry is a directory
      dirInode->refreshMetaInfo();
      metaStore->releaseDir(entryID);

      *inoutNumDirsRefreshed += 1;
   }

   if ( (DirEntryType_ISREGULARFILE(entryType)) && (fileInode) )
   {
      /* fake entries, we only need a compatible interface here, but in general we are accessing
       * not inlined inodes in the hash directories, so a complete entryInfo with a parent and
       * ownerNodeID is not required for access */
      uint16_t ownerNodeID = app->getLocalNode()->getNumID();
      std::string parentID = "FULL_REFRESHER_DIRECT_ACCESS";

      std::string fileName = "FULL_REFRESHER_UKNOWN_FNAME";

      DirEntryType entryType = DirEntryType_REGULARFILE;
      int flags = 0;

      EntryInfo entryInfo(ownerNodeID, parentID, entryID, fileName, entryType, flags);

      // release the file, as we do not really need it; parentID can be anything, as the inode
      // is definitely not inlined
      metaStore->releaseFile(parentID, fileInode);

      MsgHelperStat::refreshDynAttribs(&entryInfo, true);

      *inoutNumFilesRefreshed += 1;
   }
}
