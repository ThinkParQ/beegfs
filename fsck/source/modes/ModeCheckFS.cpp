#include "ModeCheckFS.h"

#include <common/toolkit/DisposalCleaner.h>
#include <common/toolkit/ListTk.h>
#include <common/toolkit/UnitTk.h>
#include <common/toolkit/UiTk.h>
#include <common/toolkit/ZipIterator.h>
#include <components/DataFetcher.h>
#include <components/worker/RetrieveChunksWork.h>
#include <net/msghelpers/MsgHelperRepair.h>
#include <toolkit/DatabaseTk.h>
#include <toolkit/FsckTkEx.h>

#include <program/Program.h>

#include <ftw.h>

#include <boost/lexical_cast.hpp>

template<unsigned Actions>
UserPrompter::UserPrompter(const FsckRepairAction (&possibleActions)[Actions],
   FsckRepairAction defaultRepairAction)
   : askForAction(true), possibleActions(possibleActions, possibleActions + Actions),
     repairAction(FsckRepairAction_UNDEFINED)
{
   if(Program::getApp()->getConfig()->getReadOnly() )
      askForAction = false;
   else
   if(Program::getApp()->getConfig()->getAutomatic() )
   {
      askForAction = false;
      repairAction = defaultRepairAction;
   }
}

FsckRepairAction UserPrompter::chooseAction(const std::string& prompt)
{
   if(askForAction)
      FsckTkEx::fsckOutput("> " + prompt, OutputOptions_LINEBREAK | OutputOptions_NOLOG);

   FsckTkEx::fsckOutput("> " + prompt, OutputOptions_NOSTDOUT);

   while(askForAction)
   {
      for(size_t i = 0; i < possibleActions.size(); i++)
      {
         FsckTkEx::fsckOutput(
            "   " + StringTk::uintToStr(i + 1) + ") "
               + FsckTkEx::getRepairActionDesc(possibleActions[i]), OutputOptions_NOLOG |
               OutputOptions_LINEBREAK);
      }

      for(size_t i = 0; i < possibleActions.size(); i++)
      {
         FsckTkEx::fsckOutput(
            "   " + StringTk::uintToStr(i + possibleActions.size() + 1) + ") "
               + FsckTkEx::getRepairActionDesc(possibleActions[i]) + " (apply for all)",
               OutputOptions_NOLOG | OutputOptions_LINEBREAK);
      }

      std::string inputStr;
      getline(std::cin, inputStr);

      unsigned input = StringTk::strToUInt(inputStr);

      if( (input > 0) && (input <= possibleActions.size() ) )
      {
         // user chose for this error
         repairAction = possibleActions[input - 1];
         break;
      }

      if( (input > possibleActions.size() ) && (input <= possibleActions.size() * 2) )
      {
         // user chose for all errors => do not ask again
         askForAction = false;
         repairAction = possibleActions[input - possibleActions.size() - 1];
         break;
      }
   }

   FsckTkEx::fsckOutput(" - [ra: " + FsckTkEx::getRepairActionDesc(repairAction, true) + "]",
      OutputOptions_LINEBREAK | OutputOptions_NOSTDOUT);

   return repairAction;
}

static FhgfsOpsErr handleDisposalItem(Node& owner, const std::string& entryID,
   const bool isMirrored)
{
   FhgfsOpsErr err = DisposalCleaner::unlinkFile(owner, entryID, isMirrored);
   if (err == FhgfsOpsErr_INUSE)
      return FhgfsOpsErr_SUCCESS;
   else
      return err;
}



ModeCheckFS::ModeCheckFS()
 : log("ModeCheckFS")
{
}

ModeCheckFS::~ModeCheckFS()
{
}

void ModeCheckFS::printHelp()
{
   std::cout <<
      "MODE ARGUMENTS:\n"
      " Optional:\n"
      "  --readOnly             Check only, skip repairs.\n"
      "  --runOffline           Run in offline mode, which disables the modification\n"
      "                         logging. Use this only if you are sure there is no user\n"
      "                         access to the file system while the check is running.\n"
      "  --automatic            Do not prompt for repair actions and automatically use\n"
      "                         reasonable default actions.\n"
      "  --noFetch              Do not build a new database from the servers and\n"
      "                         instead work on an existing database (e.g. from a\n"
      "                         previous read-only run).\n"
      "  --quotaEnabled         Enable checks for quota support.\n"
      "  --databasePath=<path>  Path to store for the database files. On systems with \n"
      "                         many files, the database can grow to a size of several \n"
      "                         100 GB.\n"
      "                         (Default: " CONFIG_DEFAULT_DBPATH ")\n"
      "  --overwriteDbFile      Overwrite an existing database file without prompt.\n"
      "  --ignoreDBDiskSpace    Ignore free disk space check for database file.\n"
      "  --logOutFile=<path>    Path to the fsck output file, which contains a copy of\n"
      "                         the console output.\n"
      "                         (Default: " CONFIG_DEFAULT_OUTFILE ")\n"
      "  --logStdFile=<path>    Path to the program error log file, which contains e.g.\n"
      "                         network error messages.\n"
      "                         (Default: " CONFIG_DEFAULT_LOGFILE ")\n"
      "  --forceRestart         Restart check even though another instance seems to be running.\n"
      "                         Use only if a previous run was aborted, and make sure no other\n"
      "                         fsck is running on the same file system at the same time.\n"
      "\n"
      "USAGE:\n"
      " This mode performs a full check and optional repair of a BeeGFS file system\n"
      " instance by building a database of the current file system contents on the\n"
      " local machine.\n"
      "\n"
      " The fsck gathers information from all BeeGFS server daemons in parallel through\n"
      " their configured network interfaces. All server components of the file\n"
      " system have to be running to start a check.\n"
      "\n"
      " If the fsck is running with the \"--runoffline\" argument, users may not\n"
      " access the file system during a run (otherwise false errors might be reported).\n"
      "\n"
      " Example: Check for errors, but skip repairs\n"
      "  $ beegfs-fsck --checkfs --readonly\n";

   std::cout << std::flush;
}

int ModeCheckFS::execute()
{
   App* app = Program::getApp();
   Config *cfg = app->getConfig();
   std::string databasePath = cfg->getDatabasePath();

   if ( this->checkInvalidArgs(cfg->getUnknownConfigArgs()) )
      return APPCODE_INVALID_CONFIG;

   FsckTkEx::printVersionHeader(false);
   printHeaderInformation();

   if (cfg->getNoFetch() && !Program::getApp()->getConfig()->getReadOnly()) {
      FsckTkEx::fsckOutput(
         "           WARNING:  RISK OF DATA LOSS!\n"
         "        NEVER USE THE SAME DATABASE TWICE!\n\n"
         
         "After running fsck, any database obtained previously no longer matches the state\n"
         "of the filesystem. A new database MUST be obtained before running fsck again.\n\n"
         
         "You are seeing this warning because you are using the option --nofetch.",
         OutputOptions_DOUBLELINEBREAK | OutputOptions_ADDLINEBREAKBEFORE);

      if (!uitk::userYNQuestion("Do you wish to continue?"))
         return APPCODE_USER_ABORTED;
   }


   if ( !FsckTkEx::checkReachability() )
      return APPCODE_COMMUNICATION_ERROR;

   if (!FsckTkEx::checkConsistencyStates())
   {
      FsckTkEx::fsckOutput("At least one meta or storage target is in NEEDS_RESYNC state. "
            "To prevent interference between fsck and resync operations, fsck will abort now. "
            "Please make sure that no resyncs are currently pending or running.",
            OutputOptions_DOUBLELINEBREAK | OutputOptions_ADDLINEBREAKBEFORE);
      return APPCODE_RUNTIME_ERROR;
   } 

   if(cfg->getNoFetch() )
   {
      try {
         this->database.reset(new FsckDB(databasePath + "/fsckdb", cfg->getTuneDbFragmentSize(),
            cfg->getTuneDentryCacheSize(), false) );
      } catch (const FragmentDoesNotExist& e) {
         std::string err = "Database was found to be incomplete in path " + databasePath;
         log.logErr(err);
         FsckTkEx::fsckOutput(err);
         return APPCODE_RUNTIME_ERROR;
      }
   }
   else
   {
      int initDBRes = initDatabase();
      if (initDBRes)
         return initDBRes;

      disposeUnusedFiles();

      boost::scoped_ptr<ModificationEventHandler> modificationEventHandler;

      if (!cfg->getRunOffline())
      {
         modificationEventHandler.reset(
            new ModificationEventHandler(*this->database->getModificationEventsTable() ) );
         modificationEventHandler->start();
         Program::getApp()->setModificationEventHandler(modificationEventHandler.get() );

         // start modification logging
         auto startLogRes = FsckTkEx::startModificationLogging(app->getMetaNodes(),
               app->getLocalNode(), cfg->getForceRestart());

         if (startLogRes != FhgfsOpsErr_SUCCESS)
         {
            std::string errStr;
            switch (startLogRes)
            {
               case FhgfsOpsErr_INUSE:
                  errStr = "Another instance of beegfs-fsck is still running or was aborted. "
                     "Cannot start a new one unless --forceRestart command line switch is used. "
                     "Do this only after making sure there is no other instance of beefs-fsck "
                     "running.";

                  // stop the modification event handler
                  Program::getApp()->setModificationEventHandler(NULL);
                  modificationEventHandler->selfTerminate();
                  modificationEventHandler->join();
                  break;

               default:
                  errStr = "Unable to start file system modification logging. "
                     "Fsck cannot proceed.";
            }

            log.logErr(errStr);
            FsckTkEx::fsckOutput(errStr);

            return APPCODE_RUNTIME_ERROR;
         }
      }

      FhgfsOpsErr gatherDataRes = gatherData(cfg->getForceRestart());

      if (!cfg->getRunOffline())
      {
         // stop modification logging
         bool eventLoggingOK = FsckTkEx::stopModificationLogging(app->getMetaNodes());
         // stop mod event handler (to make it flush for the last time
         Program::getApp()->setModificationEventHandler(NULL);
         modificationEventHandler->selfTerminate();
         modificationEventHandler->join();

         // if event logging is not OK (i.e. that not all events might have been processed), go
         // into read-only mode
         if ( !eventLoggingOK )
         {
            Program::getApp()->getConfig()->disableAutomaticRepairMode();
            Program::getApp()->getConfig()->setReadOnly();

            FsckTkEx::fsckOutput("-----",
               OutputOptions_ADDLINEBREAKBEFORE | OutputOptions_FLUSH | OutputOptions_LINEBREAK);
            FsckTkEx::fsckOutput(
               "WARNING: Fsck did not get all modification events from metadata servers.",
               OutputOptions_FLUSH | OutputOptions_LINEBREAK);
            FsckTkEx::fsckOutput(
               "For instance, this might have happened due to network timeouts.",
               OutputOptions_FLUSH | OutputOptions_LINEBREAK);
            FsckTkEx::fsckOutput(
               "This means beegfs-fsck is not aware of all changes in the filesystem and it is "
               "not safe to execute repair actions. "
               "In the worst case executing repair actions can result in data loss.",
               OutputOptions_FLUSH | OutputOptions_DOUBLELINEBREAK);
            FsckTkEx::fsckOutput(
               "Thus, read-only mode was automatically enabled. "
               "You can still force repair by running beegfs-fsck again on the existing "
               "database with the -noFetch option.",
               OutputOptions_FLUSH | OutputOptions_DOUBLELINEBREAK);
            FsckTkEx::fsckOutput("Please press any key to continue.",
               OutputOptions_FLUSH | OutputOptions_LINEBREAK);
            FsckTkEx::fsckOutput("-----", OutputOptions_FLUSH | OutputOptions_DOUBLELINEBREAK);

            std::cin.get();
         }

      }

      if (gatherDataRes != FhgfsOpsErr_SUCCESS)
      {
         std::string errStr;
         switch (gatherDataRes)
         {
            case FhgfsOpsErr_INUSE:
               errStr = "Another instance of beegfs-fsck is still running or was aborted. "
                  "Cannot start a new one unless --forceRestart command line switch is used. "
                  "Do this only after making sure there is no other instance of beefs-fsck "
                  "running.";
               break;
            default:
               errStr = "An error occured while fetching data from servers. Fsck cannot proceed. "
                  "Please see log file for more information.";
         }

         log.logErr(errStr);
         FsckTkEx::fsckOutput(errStr);

         return APPCODE_RUNTIME_ERROR;
      }
   }

   checkAndRepair();

   return APPCODE_NO_ERROR;
}

/*
 * initializes the database, this is done here and not inside the main app, because we do not want
 * it to effect the help modes; DB file shall only be created if user really runs a check
 *
 * @return integer value symbolizing an appcode
 */
int ModeCheckFS::initDatabase()
{
   Config* cfg = Program::getApp()->getConfig();

   // create the database path
   Path dbPath(cfg->getDatabasePath() + "/fsckdb");

   if ( !StorageTk::createPathOnDisk(dbPath, false) )
   {
      FsckTkEx::fsckOutput("Could not create path for database files: " +
         dbPath.str());
      return APPCODE_INITIALIZATION_ERROR;
   }

   // check disk space
   if ( !FsckTkEx::checkDiskSpace(dbPath) )
      return APPCODE_INITIALIZATION_ERROR;

   // check if DB file path already exists and is not empty
   if ( StorageTk::pathExists(dbPath.str())
      && StorageTk::pathHasChildren(dbPath.str()) && (!cfg->getOverwriteDbFile()) )
   {
      FsckTkEx::fsckOutput("The database path already exists: " + dbPath.str());
      FsckTkEx::fsckOutput("If you continue now any existing database files in that path will be "
         "deleted.");

      std::string input;

      while (input.size() != 1 || !strchr("YyNn", input[0]))
      {
         FsckTkEx::fsckOutput("Do you want to continue? (Y/N)");
         std::getline(std::cin, input);
      }

      switch (input[0])
      {
         case 'Y':
         case 'y':
            break; // just do nothing and go ahead
         case 'N':
         case 'n':
         default:
            // abort here
            return APPCODE_USER_ABORTED;
      }
   }

   struct ops
   {
      static int visit(const char* path, const struct stat*, int type, struct FTW* state)
      {
         if(state->level == 0)
            return 0;
         else
         if(type == FTW_F || type == FTW_SL)
            return ::unlink(path);
         else
            return ::rmdir(path);
      }
   };

   int ftwRes = ::nftw(dbPath.str().c_str(), ops::visit, 10, FTW_DEPTH | FTW_PHYS);
   if(ftwRes)
   {
      FsckTkEx::fsckOutput("Could not empty path for database files: " + dbPath.str() );
      return APPCODE_INITIALIZATION_ERROR;
   }

   try {
      this->database.reset(
         new FsckDB(dbPath.str(), cfg->getTuneDbFragmentSize(),
            cfg->getTuneDentryCacheSize(), true) );
   } catch (const std::runtime_error& e) {
      FsckTkEx::fsckOutput("Database " + dbPath.str() + " is corrupt");
      return APPCODE_RUNTIME_ERROR;
   }

   return 0;
}

void ModeCheckFS::printHeaderInformation()
{
   Config* cfg = Program::getApp()->getConfig();

   // get the current time and create some nice output, so the user can see at which time the run
   // was started (especially nice to find older runs in the log file)
   time_t t;
   time(&t);
   std::string timeStr = std::string(ctime(&t));
   FsckTkEx::fsckOutput(
      "Started BeeGFS fsck in forward check mode [" + timeStr.substr(0, timeStr.length() - 1)
         + "]\nLog will be written to " + cfg->getLogStdFile() + "\nDatabase will be saved in "
         + cfg->getDatabasePath(), OutputOptions_LINEBREAK | OutputOptions_HEADLINE);

   if (Program::getApp()->getMetaMirrorBuddyGroupMapper()->getSize() > 0)
   {
      FsckTkEx::fsckOutput(
            "IMPORTANT NOTICE: The initiation of a repair action on a metadata mirror group "
            "will temporarily set the consistency state of the secondary node to bad to prevent "
            "interaction during the repair. After fsck finishes, it will set the states "
            "of all affected nodes to needs-resync, so the repaired data will be synced from their "
            "primary. \n\nIf beegfs-fsck is aborted prematurely, a resync needs to be initiated "
            "manually so all targets are consistent (good) again." ,
            OutputOptions_DOUBLELINEBREAK | OutputOptions_HEADLINE);
   }
}

void ModeCheckFS::disposeUnusedFiles()
{
   Config* cfg = Program::getApp()->getConfig();

   if (cfg->getReadOnly() || cfg->getNoFetch())
      return;

   FsckTkEx::fsckOutput("Step 2: Delete unused files from disposal: ", OutputOptions_NONE);

   using namespace std::placeholders;

   uint64_t errors = 0;

   DisposalCleaner dc(*Program::getApp()->getMetaMirrorBuddyGroupMapper());
   dc.run(Program::getApp()->getMetaNodes()->referenceAllNodes(),
         handleDisposalItem,
         [&] (const auto&, const auto&) { errors += 1; });

   if (errors > 0)
      FsckTkEx::fsckOutput("Some files could not be deleted.");
   else
      FsckTkEx::fsckOutput("Finished.");
}

FhgfsOpsErr ModeCheckFS::gatherData(bool forceRestart)
{
   FsckTkEx::fsckOutput("Step 3: Gather data from nodes: ", OutputOptions_DOUBLELINEBREAK);

   DataFetcher dataFetcher(*this->database, forceRestart);
   const FhgfsOpsErr retVal = dataFetcher.execute();

   FsckTkEx::fsckOutput("", OutputOptions_LINEBREAK);

   return retVal;
}

template<typename Obj, typename State>
FsckErrCount ModeCheckFS::checkAndRepairGeneric(Cursor<Obj> cursor,
   void (ModeCheckFS::*repair)(Obj&, FsckErrCount&, State&), State& state)
{
   FsckErrCount errorCount;

   while(cursor.step() )
   {
      Obj* entry = cursor.get();

      (this->*repair)(*entry, errorCount, state);
   }


   if (errorCount.getTotalErrors())
   {
      database->getDentryTable()->commitChanges();
      database->getFileInodesTable()->commitChanges();
      database->getDirInodesTable()->commitChanges();
      database->getChunksTable()->commitChanges();
      database->getContDirsTable()->commitChanges();
      database->getFsIDsTable()->commitChanges();

      FsckTkEx::fsckOutput(">>> Found " + StringTk::int64ToStr(errorCount.getTotalErrors())
         + " errors. Detailed information can also be found in "
         + Program::getApp()->getConfig()->getLogOutFile() + ".",
      OutputOptions_DOUBLELINEBREAK);

   }
   else
   {
      FsckTkEx::fsckOutput(">>> None found", OutputOptions_FLUSH | OutputOptions_LINEBREAK);
   }

   return errorCount;
}

FsckErrCount ModeCheckFS::checkAndRepairDanglingDentry()
{
   FsckRepairAction fileActions[] = {
      FsckRepairAction_NOTHING,
      FsckRepairAction_DELETEDENTRY,
   };

   FsckRepairAction dirActions[] = {
      FsckRepairAction_NOTHING,
      FsckRepairAction_DELETEDENTRY,
      FsckRepairAction_CREATEDEFAULTDIRINODE,
   };

   UserPrompter forFiles(fileActions, FsckRepairAction_DELETEDENTRY);
   UserPrompter forDirs(dirActions, FsckRepairAction_CREATEDEFAULTDIRINODE);

   std::pair<UserPrompter*, UserPrompter*> prompt(&forFiles, &forDirs);

   FsckTkEx::fsckOutput("* Checking: Dangling directory entry ...",
      OutputOptions_FLUSH |OutputOptions_LINEBREAK);

   return checkAndRepairGeneric(this->database->findDanglingDirEntries(),
      &ModeCheckFS::repairDanglingDirEntry, prompt);
}

FsckErrCount ModeCheckFS::checkAndRepairWrongInodeOwner()
{
   FsckRepairAction possibleActions[] = {
      FsckRepairAction_NOTHING,
      FsckRepairAction_CORRECTOWNER,
   };

   UserPrompter prompt(possibleActions, FsckRepairAction_CORRECTOWNER);

   FsckTkEx::fsckOutput("* Checking: Wrong owner node saved in inode ...",
      OutputOptions_FLUSH | OutputOptions_LINEBREAK);

   return checkAndRepairGeneric(this->database->findInodesWithWrongOwner(),
      &ModeCheckFS::repairWrongInodeOwner, prompt);
}

FsckErrCount ModeCheckFS::checkAndRepairWrongOwnerInDentry()
{
   FsckRepairAction possibleActions[] = {
      FsckRepairAction_NOTHING,
      FsckRepairAction_CORRECTOWNER,
   };

   UserPrompter prompt(possibleActions, FsckRepairAction_CORRECTOWNER);

   FsckTkEx::fsckOutput("* Checking: Dentry points to inode on wrong node ...",
      OutputOptions_FLUSH | OutputOptions_LINEBREAK);

   return checkAndRepairGeneric(this->database->findDirEntriesWithWrongOwner(),
      &ModeCheckFS::repairWrongInodeOwnerInDentry, prompt);
}

FsckErrCount ModeCheckFS::checkAndRepairOrphanedContDir()
{
   FsckRepairAction possibleActions[] = {
      FsckRepairAction_NOTHING,
      FsckRepairAction_CREATEDEFAULTDIRINODE,
   };

   UserPrompter prompt(possibleActions, FsckRepairAction_CREATEDEFAULTDIRINODE);

   FsckTkEx::fsckOutput("* Checking: Content directory without an inode ...",
      OutputOptions_FLUSH | OutputOptions_LINEBREAK);

   return checkAndRepairGeneric(this->database->findOrphanedContDirs(),
      &ModeCheckFS::repairOrphanedContDir, prompt);
}

FsckErrCount ModeCheckFS::checkAndRepairOrphanedDirInode()
{
   FsckRepairAction possibleActions[] = {
      FsckRepairAction_NOTHING,
      FsckRepairAction_LOSTANDFOUND,
   };

   UserPrompter prompt(possibleActions, FsckRepairAction_LOSTANDFOUND);

   FsckTkEx::fsckOutput("* Checking: Dir inode without a dentry pointing to it (orphaned inode) ...",
      OutputOptions_FLUSH | OutputOptions_LINEBREAK);

   auto result = checkAndRepairGeneric(this->database->findOrphanedDirInodes(),
      &ModeCheckFS::repairOrphanedDirInode, prompt);

   releaseLostAndFound();
   return result;
}

FsckErrCount ModeCheckFS::checkAndRepairOrphanedFileInode()
{
   FsckRepairAction possibleActions[] = {
      FsckRepairAction_NOTHING,
      FsckRepairAction_DELETEINODE,
      //FsckRepairAction_LOSTANDFOUND,
   };

   UserPrompter prompt(possibleActions, FsckRepairAction_DELETEINODE);

   FsckTkEx::fsckOutput("* Checking: File inode without a dentry pointing to it (orphaned inode) ...",
      OutputOptions_FLUSH | OutputOptions_LINEBREAK);

   return checkAndRepairGeneric(this->database->findOrphanedFileInodes(),
      &ModeCheckFS::repairOrphanedFileInode, prompt);
}

FsckErrCount ModeCheckFS::checkAndRepairDuplicateInodes()
{
   FsckRepairAction possibleActions[] = {
      FsckRepairAction_NOTHING,
      FsckRepairAction_REPAIRDUPLICATEINODE,
   };

   UserPrompter prompt(possibleActions, FsckRepairAction_REPAIRDUPLICATEINODE);

   FsckTkEx::fsckOutput("* Checking: Duplicate inodes ...", OutputOptions_FLUSH | OutputOptions_LINEBREAK);

   return checkAndRepairGeneric(this->database->findDuplicateInodeIDs(),
      &ModeCheckFS::repairDuplicateInode, prompt);
}

FsckErrCount ModeCheckFS::checkAndRepairOrphanedChunk()
{
   FsckRepairAction possibleActions[] = {
      FsckRepairAction_NOTHING,
   };

   UserPrompter prompt(possibleActions, FsckRepairAction_NOTHING);
   RepairChunkState state = { &prompt, "", FsckRepairAction_UNDEFINED };

   FsckTkEx::fsckOutput("* Checking: Chunk without an inode pointing to it (orphaned chunk) ...",
      OutputOptions_FLUSH | OutputOptions_LINEBREAK);

   return checkAndRepairGeneric(this->database->findOrphanedChunks(),
      &ModeCheckFS::repairOrphanedChunk, state);
}

FsckErrCount ModeCheckFS::checkAndRepairMissingContDir()
{
   FsckRepairAction possibleActions[] = {
      FsckRepairAction_NOTHING,
      FsckRepairAction_CREATECONTDIR,
   };

   UserPrompter prompt(possibleActions, FsckRepairAction_CREATECONTDIR);

   FsckTkEx::fsckOutput("* Checking: Directory inode without a content directory ...",
      OutputOptions_FLUSH | OutputOptions_LINEBREAK);

   return checkAndRepairGeneric(this->database->findInodesWithoutContDir(),
      &ModeCheckFS::repairMissingContDir, prompt);
}

FsckErrCount ModeCheckFS::checkAndRepairWrongFileAttribs()
{
   FsckRepairAction possibleActions[] = {
      FsckRepairAction_NOTHING,
      FsckRepairAction_UPDATEATTRIBS,
   };

   UserPrompter prompt(possibleActions, FsckRepairAction_UPDATEATTRIBS);

   FsckTkEx::fsckOutput("* Checking: Attributes of file inode are wrong ...",
      OutputOptions_FLUSH | OutputOptions_LINEBREAK);

   return checkAndRepairGeneric(this->database->findWrongInodeFileAttribs(),
      &ModeCheckFS::repairWrongFileAttribs, prompt);
}

FsckErrCount ModeCheckFS::checkAndRepairWrongDirAttribs()
{
   FsckRepairAction possibleActions[] = {
      FsckRepairAction_NOTHING,
      FsckRepairAction_UPDATEATTRIBS,
   };

   UserPrompter prompt(possibleActions, FsckRepairAction_UPDATEATTRIBS);

   FsckTkEx::fsckOutput("* Checking: Attributes of dir inode are wrong ...",
      OutputOptions_FLUSH | OutputOptions_LINEBREAK);

   return checkAndRepairGeneric(this->database->findWrongInodeDirAttribs(),
      &ModeCheckFS::repairWrongDirAttribs, prompt);
}

FsckErrCount ModeCheckFS::checkAndRepairFilesWithMissingTargets()
{
   FsckRepairAction possibleActions[] = {
      FsckRepairAction_NOTHING,
      FsckRepairAction_DELETEFILE,
   };

   UserPrompter prompt(possibleActions, FsckRepairAction_NOTHING);

   FsckTkEx::fsckOutput("* Checking: File has a missing target in stripe pattern ...",
      OutputOptions_FLUSH | OutputOptions_LINEBREAK);

   return checkAndRepairGeneric(
      this->database->findFilesWithMissingStripeTargets(
         Program::getApp()->getTargetMapper(), Program::getApp()->getMirrorBuddyGroupMapper() ),
      &ModeCheckFS::repairFileWithMissingTargets, prompt);
}

FsckErrCount ModeCheckFS::checkAndRepairDirEntriesWithBrokeByIDFile()
{
   FsckRepairAction possibleActions[] = {
      FsckRepairAction_NOTHING,
      FsckRepairAction_RECREATEFSID,
   };

   UserPrompter prompt(possibleActions, FsckRepairAction_RECREATEFSID);

   FsckTkEx::fsckOutput("* Checking: Dentry-by-ID file is broken or missing ...",
      OutputOptions_FLUSH | OutputOptions_LINEBREAK);

   return checkAndRepairGeneric(this->database->findDirEntriesWithBrokenByIDFile(),
      &ModeCheckFS::repairDirEntryWithBrokenByIDFile, prompt);
}

FsckErrCount ModeCheckFS::checkAndRepairOrphanedDentryByIDFiles()
{
   FsckRepairAction possibleActions[] = {
      FsckRepairAction_NOTHING,
      FsckRepairAction_RECREATEDENTRY,
   };

   UserPrompter prompt(possibleActions, FsckRepairAction_RECREATEDENTRY);

   FsckTkEx::fsckOutput("* Checking: Dentry-by-ID file is present, but no corresponding dentry ...",
      OutputOptions_FLUSH | OutputOptions_LINEBREAK);

   return checkAndRepairGeneric(this->database->findOrphanedFsIDFiles(),
      &ModeCheckFS::repairOrphanedDentryByIDFile, prompt);
}

FsckErrCount ModeCheckFS::checkAndRepairChunksWithWrongPermissions()
{
   FsckRepairAction possibleActions[] = {
      FsckRepairAction_NOTHING,
      FsckRepairAction_FIXPERMISSIONS,
   };

   UserPrompter prompt(possibleActions, FsckRepairAction_FIXPERMISSIONS);

   FsckTkEx::fsckOutput("* Checking: Chunk has wrong permissions ...",
      OutputOptions_FLUSH | OutputOptions_LINEBREAK);

   return checkAndRepairGeneric(this->database->findChunksWithWrongPermissions(),
      &ModeCheckFS::repairChunkWithWrongPermissions, prompt);
}

// no repair at the moment
FsckErrCount ModeCheckFS::checkAndRepairChunksInWrongPath()
{
   FsckRepairAction possibleActions[] = {
      FsckRepairAction_NOTHING,
      FsckRepairAction_MOVECHUNK,
   };

   UserPrompter prompt(possibleActions, FsckRepairAction_MOVECHUNK);

   FsckTkEx::fsckOutput("* Checking: Chunk is saved in wrong path ...",
      OutputOptions_FLUSH | OutputOptions_LINEBREAK);

   return checkAndRepairGeneric(this->database->findChunksInWrongPath(),
      &ModeCheckFS::repairWrongChunkPath, prompt);
}

FsckErrCount ModeCheckFS::checkAndUpdateOldStyledHardlinks()
{
   FsckRepairAction possibleActions[] = {
      FsckRepairAction_NOTHING,
      FsckRepairAction_UPDATEOLDTYLEDHARDLINKS,
   };

   UserPrompter prompt(possibleActions, FsckRepairAction_UPDATEOLDTYLEDHARDLINKS);

   FsckTkEx::fsckOutput("* Checking: Files having an inlined inode with multiple hardlinks ...",
      OutputOptions_FLUSH | OutputOptions_LINEBREAK);

   return checkAndRepairGeneric(this->database->findFilesWithMultipleHardlinks(),
      &ModeCheckFS::updateOldStyledHardlinks, prompt);
}

void ModeCheckFS::logDuplicateInodeID(checks::DuplicatedInode& dups, int&)
{
   FsckTkEx::fsckOutput(">>> Found duplicated ID " + dups.first.str(),
      OutputOptions_LINEBREAK | OutputOptions_NOSTDOUT);

   db::EntryID entryID = dups.first;
   std::string filePath = this->database->getDentryTable()->getPathOf(entryID);
   FsckTkEx::fsckOutput("     File path: " + filePath,
      OutputOptions_LINEBREAK | OutputOptions_NOSTDOUT);

   for(const auto& it : dups.second)
   {
      std::string parentDirID = it.getParentDirID();
      unsigned ownerNodeID = it.getSaveNodeID();
      bool isInlined = it.getIsInlined();
      bool isBuddyMirrored = it.getIsBuddyMirrored();

      std::string metaFilePath;
      if (isInlined)
      {
         std::string dentriesPath;
         dentriesPath += isBuddyMirrored ? META_BUDDYMIRROR_SUBDIR_NAME : "";
         dentriesPath += std::string("/") + META_DENTRIES_SUBDIR_NAME;

         metaFilePath = StorageTk::getHashPath(dentriesPath, parentDirID, META_DENTRIES_LEVEL1_SUBDIR_NUM, META_DENTRIES_LEVEL2_SUBDIR_NUM);
         metaFilePath += std::string("/") + META_DIRENTRYID_SUB_STR + "/" + entryID.str();
      }
      else
      {
         std::string inodesPath;
         inodesPath += isBuddyMirrored ? META_BUDDYMIRROR_SUBDIR_NAME : "";
         inodesPath += std::string("/") + META_INODES_SUBDIR_NAME;

         metaFilePath = StorageTk::getHashPath(inodesPath, entryID.str(), META_INODES_LEVEL1_SUBDIR_NUM, META_INODES_LEVEL2_SUBDIR_NUM);
      }

      FsckTkEx::fsckOutput("   * Found on " + std::string(isBuddyMirrored ? "buddy group " : "node ")
            + StringTk::uintToStr(ownerNodeID) + "; Metapath: " + metaFilePath, OutputOptions_LINEBREAK | OutputOptions_NOSTDOUT);
   }
}

FsckErrCount ModeCheckFS::checkDuplicateChunks()
{
   FsckTkEx::fsckOutput("* Checking: Duplicated chunks ...", OutputOptions_FLUSH | OutputOptions_LINEBREAK);

   int dummy = 0;
   return checkAndRepairGeneric(this->database->findDuplicateChunks(),
      &ModeCheckFS::logDuplicateChunk, dummy);
}

void ModeCheckFS::logDuplicateChunk(std::list<FsckChunk>& dups, FsckErrCount& errCount, int&)
{
   FsckTkEx::fsckOutput(">>> Found duplicated Chunks for ID " + dups.begin()->getID(),
      OutputOptions_LINEBREAK | OutputOptions_NOSTDOUT);

   errCount.unfixableErrors++;

   for(std::list<FsckChunk>::iterator it = dups.begin(), end = dups.end();
         it != end; ++it)
   {
      FsckTkEx::fsckOutput("   * Found on target " + StringTk::uintToStr(it->getTargetID() )
         + (it->getBuddyGroupID()
               ? ", buddy group " + StringTk::uintToStr(it->getBuddyGroupID() )
               : "")
         + " in path " + it->getSavedPath()->str(),
         OutputOptions_LINEBREAK | OutputOptions_NOSTDOUT);
   }
}

FsckErrCount ModeCheckFS::checkDuplicateContDirs()
{
   FsckTkEx::fsckOutput("* Checking: Duplicated content directores ...",
         OutputOptions_FLUSH | OutputOptions_LINEBREAK);

   int dummy = 0;
   return checkAndRepairGeneric(this->database->findDuplicateContDirs(),
      &ModeCheckFS::logDuplicateContDir, dummy);
}

void ModeCheckFS::logDuplicateContDir(std::list<db::ContDir>& dups, FsckErrCount& errCount, int&)
{
   errCount.unfixableErrors++;

   FsckTkEx::fsckOutput(">>> Found duplicated content directories for ID " + dups.front().id.str(),
      OutputOptions_LINEBREAK | OutputOptions_NOSTDOUT);
   for (auto it = dups.begin(), end = dups.end(); it != end; ++it)
   {
      FsckTkEx::fsckOutput("   * Found on " +
            std::string(it->isBuddyMirrored ? "buddy group " : "node ") +
            StringTk::uintToStr(it->saveNodeID),
         OutputOptions_LINEBREAK | OutputOptions_NOSTDOUT);
   }
}

FsckErrCount ModeCheckFS::checkMismirroredDentries()
{
   FsckTkEx::fsckOutput("* Checking: Bad target mirror information in dentry ...",
         OutputOptions_FLUSH | OutputOptions_LINEBREAK);

   int dummy = 0;
   return checkAndRepairGeneric(this->database->findMismirroredDentries(),
      &ModeCheckFS::logMismirroredDentry, dummy);
}

void ModeCheckFS::logMismirroredDentry(db::DirEntry& entry, FsckErrCount& errCount, int&)
{
   errCount.unfixableErrors++;

   FsckTkEx::fsckOutput(">>> Found mismirrored dentry " +
         database->getDentryTable()->getNameOf(entry) + " on " +
         (entry.isBuddyMirrored ? "buddy group " : "node ") +
         StringTk::uintToStr(entry.saveNodeID) + " " + StringTk::uintToStr(entry.entryOwnerNodeID));
}

FsckErrCount ModeCheckFS::checkMismirroredDirectories()
{
   FsckTkEx::fsckOutput("* Checking: Bad content mirror information in dir inode ...",
         OutputOptions_FLUSH | OutputOptions_LINEBREAK);

   int dummy = 0;
   return checkAndRepairGeneric(this->database->findMismirroredDirectories(),
      &ModeCheckFS::logMismirroredDirectory, dummy);
}

void ModeCheckFS::logMismirroredDirectory(db::DirInode& dir, FsckErrCount& errCount, int&)
{
   errCount.unfixableErrors++;

   FsckTkEx::fsckOutput(">>> Found mismirrored directory " + dir.id.str() + " on " +
         (dir.isBuddyMirrored ? "buddy group " : "node ") +
         StringTk::uintToStr(dir.saveNodeID));
;
}

FsckErrCount ModeCheckFS::checkMismirroredFiles()
{
   FsckTkEx::fsckOutput("* Checking: Bad content mirror information in file inode ...",
         OutputOptions_FLUSH | OutputOptions_LINEBREAK);

   int dummy = 0;
   return checkAndRepairGeneric(this->database->findMismirroredFiles(),
      &ModeCheckFS::logMismirroredFile, dummy);
}

void ModeCheckFS::logMismirroredFile(db::FileInode& file, FsckErrCount& errCount, int&)
{
   errCount.unfixableErrors++;

   FsckTkEx::fsckOutput(">>> Found mismirrored file " + file.id.str() + " on " +
         std::string(file.isBuddyMirrored ? "buddy group " : "node ") +
         StringTk::uintToStr(file.saveNodeID));
}

FsckErrCount ModeCheckFS::checkAndRepairMalformedChunk()
{
   FsckRepairAction possibleActions[] = {
      FsckRepairAction_NOTHING,
      FsckRepairAction_DELETECHUNK,
   };

   UserPrompter prompt(possibleActions, FsckRepairAction_DELETECHUNK);

   FsckTkEx::fsckOutput("* Checking: Malformed chunk ...", OutputOptions_FLUSH | OutputOptions_LINEBREAK);

   return checkAndRepairGeneric(Cursor<FsckChunk>(database->getMalformedChunksList()->cursor()),
      &ModeCheckFS::repairMalformedChunk, prompt);
}

void ModeCheckFS::repairMalformedChunk(FsckChunk& chunk, FsckErrCount& errCount, UserPrompter& prompt)
{
   errCount.fixableErrors++;

   FsckRepairAction action = prompt.chooseAction("Chunk ID: " + chunk.getID() + " on " +
         (chunk.getBuddyGroupID()
            ? "buddy group " + StringTk::uintToStr(chunk.getBuddyGroupID())
            : "target " + StringTk::uintToStr(chunk.getTargetID())));

   switch (action)
   {
   case FsckRepairAction_UNDEFINED:
   case FsckRepairAction_NOTHING:
      break;

   case FsckRepairAction_DELETECHUNK: {
      FhgfsOpsErr refErr;
      NodeStore* storageNodeStore = Program::getApp()->getStorageNodes();
      auto node = storageNodeStore->referenceNodeByTargetID(chunk.getTargetID(),
         Program::getApp()->getTargetMapper(), &refErr);

      if(!node)
      {
         FsckTkEx::fsckOutput("could not get storage target "
            + StringTk::uintToStr(chunk.getTargetID() ), OutputOptions_LINEBREAK);
         return;
      }

      FsckChunkList chunks(1, chunk);
      FsckChunkList failed;

      MsgHelperRepair::deleteChunks(*node, &chunks, &failed);

      break;
   }

   default:
      throw std::runtime_error("bad repair action");
   }
}

void ModeCheckFS::checkAndRepair()
{
   FsckTkEx::fsckOutput("Step 4: Check for errors... ", OutputOptions_DOUBLELINEBREAK);

   Config* cfg = Program::getApp()->getConfig();

   FsckErrCount errorCount;

   errorCount += checkAndRepairDuplicateInodes();
   errorCount += checkDuplicateChunks();
   errorCount += checkDuplicateContDirs();
   errorCount += checkMismirroredDentries();
   errorCount += checkMismirroredDirectories();
   errorCount += checkMismirroredFiles();

   if (errorCount.unfixableErrors)
   {
      FsckTkEx::fsckOutput("Found errors beegfs-fsck cannot fix. Please consult the log for "
         "more information.", OutputOptions_LINEBREAK);
      return;
   }

   std::bitset<CHECK_FS_ACTIONS_COUNT> checkFsActions = cfg->getCheckFsActions();
   if (checkFsActions.none())
   {
      // enable all checks if user didn't specified specific checks to run
      checkFsActions.set();
   }

   if (checkFsActions.test(CHECK_MALFORMED_CHUNK))
      errorCount += checkAndRepairMalformedChunk();

   if (checkFsActions.test(CHECK_FILES_WITH_MISSING_TARGETS))
      errorCount += checkAndRepairFilesWithMissingTargets();

   if (checkFsActions.test(CHECK_ORPHANED_DENTRY_BYIDFILES))
      errorCount += checkAndRepairOrphanedDentryByIDFiles();

   if (checkFsActions.test(CHECK_DIRENTRIES_WITH_BROKENIDFILE))
      errorCount += checkAndRepairDirEntriesWithBrokeByIDFile();

   if (checkFsActions.test(CHECK_ORPHANED_CHUNK))
      errorCount += checkAndRepairOrphanedChunk();

   if (checkFsActions.test(CHECK_CHUNKS_IN_WRONGPATH))
      errorCount += checkAndRepairChunksInWrongPath();

   if (checkFsActions.test(CHECK_WRONG_INODE_OWNER))
      errorCount += checkAndRepairWrongInodeOwner();

   if (checkFsActions.test(CHECK_WRONG_OWNER_IN_DENTRY))
      errorCount += checkAndRepairWrongOwnerInDentry();

   if (checkFsActions.test(CHECK_ORPHANED_CONT_DIR))
      errorCount += checkAndRepairOrphanedContDir();

   if (checkFsActions.test(CHECK_ORPHANED_DIR_INODE))
      errorCount += checkAndRepairOrphanedDirInode();

   if (checkFsActions.test(CHECK_ORPHANED_FILE_INODE))
      errorCount += checkAndRepairOrphanedFileInode();

   if (checkFsActions.test(CHECK_DANGLING_DENTRY))
      errorCount += checkAndRepairDanglingDentry();

   if (checkFsActions.test(CHECK_MISSING_CONT_DIR))
      errorCount += checkAndRepairMissingContDir();

   if (checkFsActions.test(CHECK_WRONG_FILE_ATTRIBS))
      errorCount += checkAndRepairWrongFileAttribs();

   if (checkFsActions.test(CHECK_WRONG_DIR_ATTRIBS))
      errorCount += checkAndRepairWrongDirAttribs();

   if (checkFsActions.test(CHECK_OLD_STYLED_HARDLINKS))
      errorCount += checkAndUpdateOldStyledHardlinks();

   if ( cfg->getQuotaEnabled())
   {
      errorCount += checkAndRepairChunksWithWrongPermissions();
   }

   if ( cfg->getReadOnly() )
   {
      FsckTkEx::fsckOutput(
         "Found " + StringTk::int64ToStr(errorCount.getTotalErrors())
            + " errors. Detailed information can also be found in "
            + Program::getApp()->getConfig()->getLogOutFile() + ".",
         OutputOptions_ADDLINEBREAKBEFORE | OutputOptions_LINEBREAK);
      return;
   }

   for (auto it = secondariesSetBad.begin(); it != secondariesSetBad.end(); ++it)
   {
      auto secondary = *it;

      FsckTkEx::fsckOutput(">>> Setting metadata node " + StringTk::intToStr(secondary.val())
            + " to needs-resync", OutputOptions_LINEBREAK);

      auto setRes = MsgHelperRepair::setNodeState(secondary, TargetConsistencyState_NEEDS_RESYNC);
      if (setRes != FhgfsOpsErr_SUCCESS)
         FsckTkEx::fsckOutput("Failed: " + boost::lexical_cast<std::string>(setRes),
               OutputOptions_LINEBREAK);
   }

   if (errorCount.getTotalErrors())
      FsckTkEx::fsckOutput(">>> Found " + StringTk::int64ToStr(errorCount.getTotalErrors()) + " errors <<< ",
         OutputOptions_ADDLINEBREAKBEFORE | OutputOptions_LINEBREAK);
}

//////////////////////////
// internals      ///////
/////////////////////////
void ModeCheckFS::repairDanglingDirEntry(db::DirEntry& entry, FsckErrCount& errCount,
   std::pair<UserPrompter*, UserPrompter*>& prompt)
{
   errCount.fixableErrors++;

   FsckDirEntry fsckEntry = entry;

   FsckRepairAction action;
   std::string promptText = "Entry ID: " + fsckEntry.getID() + "; Path: " +
         this->database->getDentryTable()->getPathOf(entry) + "; " +
         (entry.isBuddyMirrored ? "Buddy group: " : "Node: ") +
         StringTk::uintToStr(entry.saveNodeID);

   if(fsckEntry.getEntryType() == FsckDirEntryType_DIRECTORY)
      action = prompt.second->chooseAction(promptText);
   else
      action = prompt.first->chooseAction(promptText);

   fsckEntry.setName(this->database->getDentryTable()->getNameOf(entry) );

   FsckDirEntryList entries(1, fsckEntry);
   FsckDirEntryList failedEntries;

   switch(action)
   {
   case FsckRepairAction_UNDEFINED:
   case FsckRepairAction_NOTHING:
      break;

   case FsckRepairAction_DELETEDENTRY: {
      MsgHelperRepair::deleteDanglingDirEntries(fsckEntry.getSaveNodeID(),
            fsckEntry.getIsBuddyMirrored(), &entries, &failedEntries,
            secondariesSetBad);

      if(failedEntries.empty() )
      {
         this->database->getDentryTable()->remove(entries);
         this->deleteFsIDsFromDB(entries);
      }

      break;
   }

   case FsckRepairAction_CREATEDEFAULTDIRINODE: {
      FsckDirInodeList createdInodes;

      // create mirrored inodes iff the dentry was mirrored. if a contdir with the same id exists,
      // a previous check will have created an inode for it, leaving this dentry not dangling.
      MsgHelperRepair::createDefDirInodes(fsckEntry.getInodeOwnerNodeID(), fsckEntry.getIsBuddyMirrored(),
            {std::make_tuple(fsckEntry.getID(), fsckEntry.getIsBuddyMirrored())}, &createdInodes,
            secondariesSetBad);

      this->database->getDirInodesTable()->insert(createdInodes);

      break;
   }

   default:
      throw std::runtime_error("bad repair action");
   }
}

void ModeCheckFS::repairWrongInodeOwner(FsckDirInode& inode, FsckErrCount& errCount, UserPrompter& prompt)
{
   errCount.fixableErrors++;

   FsckRepairAction action = prompt.chooseAction("Entry ID: " + inode.getID()
      + "; Path: "
      + this->database->getDentryTable()->getPathOf(db::EntryID::fromStr(inode.getID() ) ) );

   switch(action)
   {
   case FsckRepairAction_UNDEFINED:
   case FsckRepairAction_NOTHING:
      break;

   case FsckRepairAction_CORRECTOWNER: {
      inode.setOwnerNodeID(inode.getSaveNodeID() );

      FsckDirInodeList inodes(1, inode);
      FsckDirInodeList failed;

      MsgHelperRepair::correctInodeOwners(inode.getSaveNodeID(), inode.getIsBuddyMirrored(),
            &inodes, &failed, secondariesSetBad);

      if(failed.empty() )
         this->database->getDirInodesTable()->update(inodes);

      break;
   }

   default:
      throw std::runtime_error("bad repair action");
   }
}

void ModeCheckFS::repairWrongInodeOwnerInDentry(std::pair<db::DirEntry, NumNodeID>& error,
   FsckErrCount& errCount, UserPrompter& prompt)
{
   errCount.fixableErrors++;

   FsckDirEntry fsckEntry = error.first;

   FsckRepairAction action = prompt.chooseAction("File ID: " + fsckEntry.getID()
      + "; Path: " + this->database->getDentryTable()->getPathOf(error.first) );

   switch(action)
   {
   case FsckRepairAction_UNDEFINED:
   case FsckRepairAction_NOTHING:
      break;

   case FsckRepairAction_CORRECTOWNER: {
      fsckEntry.setName(this->database->getDentryTable()->getNameOf(error.first) );

      FsckDirEntryList dentries(1, fsckEntry);
      NumNodeIDList owners(1, NumNodeID(error.second) );
      FsckDirEntryList failed;

      MsgHelperRepair::correctInodeOwnersInDentry(fsckEntry.getSaveNodeID(),
            fsckEntry.getIsBuddyMirrored(), &dentries, &owners, &failed,
            secondariesSetBad);

      if(failed.empty() )
         this->database->getDentryTable()->updateFieldsExceptParent(dentries);

      break;
   }

   default:
      throw std::runtime_error("bad repair action");
   }
}

void ModeCheckFS::repairOrphanedDirInode(FsckDirInode& inode, FsckErrCount& errCount, UserPrompter& prompt)
{
   errCount.fixableErrors++;

   FsckRepairAction action = prompt.chooseAction("Directory ID: " + inode.getID() + "; " +
         (inode.getIsBuddyMirrored() ? "Buddy group: " : "Node: ") + inode.getSaveNodeID().str());

   switch(action)
   {
   case FsckRepairAction_UNDEFINED:
   case FsckRepairAction_NOTHING:
      break;

   case FsckRepairAction_LOSTANDFOUND: {
      if(!ensureLostAndFoundExists() )
      {
         log.logErr("Orphaned dir inodes could not be linked to lost+found, because lost+found "
            "directory could not be created");
         return;
      }

      FsckDirInodeList inodes(1, inode);
      FsckDirEntryList created;
      FsckDirInodeList failed;

      MsgHelperRepair::linkToLostAndFound(*this->lostAndFoundNode, &this->lostAndFoundInfo, &inodes,
         &failed, &created, secondariesSetBad);

      if(failed.empty() )
         this->database->getDentryTable()->insert(created);

      // on server side, each dentry also created a dentry-by-ID file
      FsckFsIDList createdFsIDs;
      for(FsckDirEntryListIter iter = created.begin(); iter != created.end(); iter++)
      {
         FsckFsID fsID(iter->getID(), iter->getParentDirID(), iter->getSaveNodeID(),
            iter->getSaveDevice(), iter->getSaveInode(), iter->getIsBuddyMirrored());
         createdFsIDs.push_back(fsID);
      }

      this->database->getFsIDsTable()->insert(createdFsIDs);
      break;
   }

   default:
      throw std::runtime_error("bad repair action");
   }
}

void ModeCheckFS::repairOrphanedFileInode(FsckFileInode& inode, FsckErrCount& errCount, UserPrompter& prompt)
{
   errCount.fixableErrors++;

   FsckRepairAction action = prompt.chooseAction("File ID: " + inode.getID() + "; " +
         (inode.getIsBuddyMirrored() ? "Buddy group: " : "Node: ") + inode.getSaveNodeID().str());


   switch(action)
   {
   case FsckRepairAction_UNDEFINED:
   case FsckRepairAction_NOTHING:
      break;

   case FsckRepairAction_DELETEINODE: {
      FsckFileInodeList inodes(1, inode);
      StringList failed;

      MsgHelperRepair::deleteFileInodes(inode.getSaveNodeID(), inode.getIsBuddyMirrored(), inodes,
            failed, secondariesSetBad);

      if(failed.empty() )
         this->database->getFileInodesTable()->remove(inodes);

      break;
   }

   default:
      throw std::runtime_error("bad repair action");
   }
}

void ModeCheckFS::repairDuplicateInode(checks::DuplicatedInode& dupInode, FsckErrCount& errCount,
   UserPrompter& prompt)
{
   int dummy = 0;
   logDuplicateInodeID(dupInode, dummy);

   // repair is not possible using FSCK for duplicate inodes if:
   // (a) inodes are of type directory OR
   // (b) inodes exists on different metadata nodes
   // (c) both inodes are inlined

   bool isDir = false;
   for (const auto& it : dupInode.second)
   {
      if (it.getDirEntryType() == DirEntryType_DIRECTORY)
      {
         isDir = true;
         break;
      }
   }

   std::set<unsigned> nodeIDs;
   for (const auto& it : dupInode.second)
      nodeIDs.insert(it.getSaveNodeID());

   bool bothInlined = true;
   for (const auto& it : dupInode.second)
      bothInlined &= it.getIsInlined();

   if (nodeIDs.size() != 1 || isDir || bothInlined)
   {
      errCount.unfixableErrors++;
      FsckTkEx::fsckOutput("> Repair is not allowed for above case using fsck. Please contact support team.",
         OutputOptions_LINEBREAK | OutputOptions_NOSTDOUT);
      return;
   }
   else
   {
      errCount.fixableErrors++;
   }

   FsckRepairAction action = prompt.chooseAction("EntryID: " + dupInode.first.str());

   FsckDuplicateInodeInfo item;

   for (const auto& it : dupInode.second)
   {
      // select item from duplicate inode list which has inlined flag
      // set (because parent's entryID is available (non-empty) there)
      if (it.getIsInlined())
      {
         item = it;
         break;
      }
   }

   switch (action)
   {
   case FsckRepairAction_UNDEFINED:
   case FsckRepairAction_NOTHING:
      break;

   case FsckRepairAction_REPAIRDUPLICATEINODE:
   {
      FsckDuplicateInodeInfoVector dupInodes{item};
      StringList failedEntries;

      MsgHelperRepair::deleteDuplicateFileInodes(NumNodeID(item.getSaveNodeID()),
         item.getIsBuddyMirrored(), dupInodes, failedEntries, secondariesSetBad);

      if (failedEntries.empty())
      {
         auto res = this->database->getFileInodesTable()->get(dupInode.first.str());

         if (res.first)
         {
            auto inode = res.second;
            FsckFileInode fsckInode = inode.toInodeWithoutStripes();
            fsckInode.setIsInlined(true);

            // now set all stripe targets into FsckFileInode object
            fsckInode.setStripeTargets(
               this->database->getFileInodesTable()->getStripeTargetsByKey(dupInode.first)
            );

            FsckFileInodeList fsckInodeList{fsckInode};
            this->database->getFileInodesTable()->update(fsckInodeList);
         }
      }

      break;
   }

   default:
      throw std::runtime_error("bad repair action");
   }
}

void ModeCheckFS::repairOrphanedChunk(FsckChunk& chunk, FsckErrCount& errCount, RepairChunkState& state)
{
   errCount.fixableErrors++;

   // ask for repair action only once per chunk id, not once per chunk id and node
   if(state.lastID != chunk.getID() )
      state.lastChunkAction = state.prompt->chooseAction("Chunk ID: " + chunk.getID() + " on " +
            (chunk.getBuddyGroupID()
               ? "buddy group " + StringTk::uintToStr(chunk.getBuddyGroupID())
               : "target " + StringTk::uintToStr(chunk.getTargetID())));

   state.lastID = chunk.getID();

   switch(state.lastChunkAction)
   {
   case FsckRepairAction_UNDEFINED:
   case FsckRepairAction_NOTHING:
      break;

   case FsckRepairAction_DELETECHUNK: {
      FhgfsOpsErr refErr;
      NodeStore* storageNodeStore = Program::getApp()->getStorageNodes();
      auto node = storageNodeStore->referenceNodeByTargetID(chunk.getTargetID(),
         Program::getApp()->getTargetMapper(), &refErr);

      if(!node)
      {
         FsckTkEx::fsckOutput("could not get storage target "
            + StringTk::uintToStr(chunk.getTargetID() ), OutputOptions_LINEBREAK);
         return;
      }

      FsckChunkList chunks(1, chunk);
      FsckChunkList failed;

      MsgHelperRepair::deleteChunks(*node, &chunks, &failed);

      if(failed.empty() )
         this->database->getChunksTable()->remove(
               {db::EntryID::fromStr(chunk.getID()), chunk.getTargetID(), chunk.getBuddyGroupID()});

      break;
   }

   default:
      throw std::runtime_error("bad repair action");
   }
}

void ModeCheckFS::repairMissingContDir(FsckDirInode& inode, FsckErrCount& errCount,
   UserPrompter& prompt)
{
   errCount.fixableErrors++;

   FsckRepairAction action = prompt.chooseAction("Directory ID: " + inode.getID() +
      "; Path: " +
      this->database->getDentryTable()->getPathOf(db::EntryID::fromStr(inode.getID())) + "; " +
      (inode.getIsBuddyMirrored() ? "Buddy group: " : "Node: ") + inode.getSaveNodeID().str());


   switch(action)
   {
   case FsckRepairAction_UNDEFINED:
   case FsckRepairAction_NOTHING:
      break;

   case FsckRepairAction_CREATECONTDIR: {
      FsckDirInodeList inodes(1, inode);
      StringList failed;

      MsgHelperRepair::createContDirs(inode.getSaveNodeID(), inode.getIsBuddyMirrored(), &inodes,
            &failed, secondariesSetBad);

      if(failed.empty() )
      {
         // create a list of cont dirs from dir inode
         FsckContDirList contDirs(1,
            FsckContDir(inode.getID(), inode.getSaveNodeID(), inode.getIsBuddyMirrored()));

         this->database->getContDirsTable()->insert(contDirs);
      }

      break;
   }

   default:
      throw std::runtime_error("bad repair action");
   }
}

void ModeCheckFS::repairOrphanedContDir(FsckContDir& dir, FsckErrCount& errCount, UserPrompter& prompt)
{
   errCount.fixableErrors++;

   FsckRepairAction action = prompt.chooseAction("Directory ID: " + dir.getID() + "; " +
         (dir.getIsBuddyMirrored() ? "Buddy group: " : "Node: ") + dir.getSaveNodeID().str());


   switch(action)
   {
   case FsckRepairAction_UNDEFINED:
   case FsckRepairAction_NOTHING:
      break;

   case FsckRepairAction_CREATEDEFAULTDIRINODE: {
      FsckDirInodeList createdInodes;
      MsgHelperRepair::createDefDirInodes(dir.getSaveNodeID(), dir.getIsBuddyMirrored(),
            {std::make_tuple(dir.getID(), dir.getIsBuddyMirrored())}, &createdInodes,
            secondariesSetBad);

      this->database->getDirInodesTable()->insert(createdInodes);

      break;
   }

   default:
      throw std::runtime_error("bad repair action");
   }
}

void ModeCheckFS::repairWrongFileAttribs(std::pair<FsckFileInode, checks::OptionalInodeAttribs>& error,
   FsckErrCount& errCount, UserPrompter& prompt)
{
   errCount.fixableErrors++;

   FsckRepairAction action = prompt.chooseAction("File ID: " + error.first.getID() + "; Path: " +
         this->database->getDentryTable()->getPathOf(db::EntryID::fromStr(error.first.getID())) +
         "; " + (error.first.getIsBuddyMirrored() ? "Buddy group: " : "Node: ") +
         error.first.getSaveNodeID().str());


   switch(action)
   {
   case FsckRepairAction_UNDEFINED:
   case FsckRepairAction_NOTHING:
      break;

   case FsckRepairAction_UPDATEATTRIBS: {
      if (error.second.size)
         error.first.setFileSize(*error.second.size);
      if (error.second.nlinks)
         error.first.setNumHardLinks(*error.second.nlinks);

      FsckFileInodeList inodes(1, error.first);
      FsckFileInodeList failed;

      MsgHelperRepair::updateFileAttribs(error.first.getSaveNodeID(),
            error.first.getIsBuddyMirrored(), &inodes, &failed, secondariesSetBad);

      if(failed.empty() )
         this->database->getFileInodesTable()->update(inodes);

      break;
   }

   default:
      throw std::runtime_error("bad repair action");
   }
}

void ModeCheckFS::repairWrongDirAttribs(std::pair<FsckDirInode, checks::InodeAttribs>& error,
   FsckErrCount& errCount, UserPrompter& prompt)
{
   errCount.fixableErrors++;

   std::string filePath = this->database->getDentryTable()->getPathOf(
      db::EntryID::fromStr(error.first.getID() ) );
   FsckRepairAction action = prompt.chooseAction("Directory ID: " + error.first.getID() +
         "; Path: " + filePath + "; " +
         (error.first.getIsBuddyMirrored() ? "Buddy group: " : "Node: ") +
         error.first.getSaveNodeID().str());


   switch(action)
   {
   case FsckRepairAction_UNDEFINED:
   case FsckRepairAction_NOTHING:
      break;

   case FsckRepairAction_UPDATEATTRIBS: {
      FsckDirInodeList inodes(1, error.first);
      FsckDirInodeList failed;

      // update the file attribs in the inode objects (even though they may not be used
      // on the server side, we need updated values here to update the DB
      error.first.setSize(error.second.size);
      error.first.setNumHardLinks(error.second.nlinks);

      MsgHelperRepair::updateDirAttribs(error.first.getSaveNodeID(),
            error.first.getIsBuddyMirrored(), &inodes, &failed, secondariesSetBad);

      if(failed.empty() )
         this->database->getDirInodesTable()->update(inodes);

      break;
   }

   default:
      throw std::runtime_error("bad repair action");
   }
}

void ModeCheckFS::repairFileWithMissingTargets(db::DirEntry& entry, FsckErrCount& errCount, UserPrompter& prompt)
{
   errCount.fixableErrors++;

   FsckDirEntry fsckEntry = entry;

   FsckRepairAction action = prompt.chooseAction("Entry ID: " + fsckEntry.getID() +
         "; Path: " + this->database->getDentryTable()->getPathOf(entry) + "; " +
         (entry.isBuddyMirrored ? "Buddy group: " : "Node: ") +
         StringTk::uintToStr(entry.entryOwnerNodeID));


   switch(action)
   {
   case FsckRepairAction_UNDEFINED:
   case FsckRepairAction_NOTHING:
      return;

   case FsckRepairAction_DELETEFILE: {
      fsckEntry.setName(this->database->getDentryTable()->getNameOf(entry) );

      FsckDirEntryList dentries(1, fsckEntry);
      FsckDirEntryList failed;

      MsgHelperRepair::deleteFiles(fsckEntry.getSaveNodeID(), fsckEntry.getIsBuddyMirrored(),
            &dentries, &failed);

      if(failed.empty() )
         this->deleteFilesFromDB(dentries);

      break;
   }

   default:
      throw std::runtime_error("bad repair action");
   }
}

void ModeCheckFS::repairDirEntryWithBrokenByIDFile(db::DirEntry& entry, FsckErrCount& errCount, UserPrompter& prompt)
{
   errCount.fixableErrors++;

   FsckDirEntry fsckEntry = entry;

   FsckRepairAction action = prompt.chooseAction("Entry ID: " + fsckEntry.getID() +
         "; Path: " + this->database->getDentryTable()->getPathOf(entry) + "; " +
         (entry.isBuddyMirrored ? "Buddy group: " : "Node: ") +
         StringTk::uintToStr(entry.entryOwnerNodeID));

   switch(action)
   {
   case FsckRepairAction_UNDEFINED:
   case FsckRepairAction_NOTHING:
      break;

   case FsckRepairAction_RECREATEFSID: {
      fsckEntry.setName(this->database->getDentryTable()->getNameOf(entry) );

      FsckDirEntryList dentries(1, fsckEntry);
      FsckDirEntryList failed;

      MsgHelperRepair::recreateFsIDs(fsckEntry.getSaveNodeID(), fsckEntry.getIsBuddyMirrored(),
            &dentries, &failed, secondariesSetBad);

      if(failed.empty() )
      {
         // create a FsID list from the dentry
         FsckFsIDList idList(1,
            FsckFsID(fsckEntry.getID(), fsckEntry.getParentDirID(), fsckEntry.getSaveNodeID(),
               fsckEntry.getSaveDevice(), fsckEntry.getSaveInode(),
               fsckEntry.getIsBuddyMirrored()));

         this->database->getFsIDsTable()->insert(idList);
      }

      break;
   }

   default:
      throw std::runtime_error("bad repair action");
   }
}

void ModeCheckFS::repairOrphanedDentryByIDFile(FsckFsID& id, FsckErrCount& errCount, UserPrompter& prompt)
{
   errCount.fixableErrors++;

   FsckRepairAction action = prompt.chooseAction(
         "Entry ID: " + id.getID() +
         "; Path: " +
         this->database->getDentryTable()->getPathOf(db::EntryID::fromStr(id.getID())) + "; " +
         (id.getIsBuddyMirrored() ? "Buddy group: " : "Node: ") + id.getSaveNodeID().str());


   switch(action)
   {
   case FsckRepairAction_UNDEFINED:
   case FsckRepairAction_NOTHING:
      break;

   case FsckRepairAction_RECREATEDENTRY: {
      FsckFsIDList fsIDs(1, id);
      FsckFsIDList failed;
      FsckDirEntryList createdDentries;
      FsckFileInodeList createdInodes;

      MsgHelperRepair::recreateDentries(id.getSaveNodeID(), id.getIsBuddyMirrored(), &fsIDs,
            &failed, &createdDentries, &createdInodes, secondariesSetBad);

      if(failed.empty() )
      {
         this->database->getDentryTable()->insert(createdDentries);
         this->database->getFileInodesTable()->insert(createdInodes);
      }

      break;
   }

   default:
      throw std::runtime_error("bad repair action");
   }
}

void ModeCheckFS::repairChunkWithWrongPermissions(std::pair<FsckChunk, FsckFileInode>& error,
   FsckErrCount& errCount, UserPrompter& prompt)
{
   errCount.fixableErrors++;
   FsckRepairAction action = prompt.chooseAction(
         "Chunk ID: " + error.first.getID() + "; "
         + (error.first.getBuddyGroupID()
         ? "Buddy group: " + StringTk::uintToStr(error.first.getBuddyGroupID())
         : "Target: " + StringTk::uintToStr(error.first.getTargetID()))
         + "; File path: "
         + this->database->getDentryTable()->getPathOf(db::EntryID::fromStr(error.first.getID())));

   switch(action)
   {
   case FsckRepairAction_UNDEFINED:
   case FsckRepairAction_NOTHING:
      break;

   case FsckRepairAction_FIXPERMISSIONS: {
      NodeStore* nodeStore = Program::getApp()->getStorageNodes();
      TargetMapper* targetMapper = Program::getApp()->getTargetMapper();

      // we will need the PathInfo later to send the SetAttr message and we don't have it in the
      // chunk
      PathInfoList pathInfoList(1, *error.second.getPathInfo() );

      // set correct permissions
      error.first.setUserID(error.second.getUserID() );
      error.first.setGroupID(error.second.getGroupID() );

      FsckChunkList chunkList(1, error.first);
      FsckChunkList failed;

      auto storageNode = nodeStore->referenceNode(
         targetMapper->getNodeID(error.first.getTargetID() ) );
      MsgHelperRepair::fixChunkPermissions(*storageNode, chunkList, pathInfoList, failed);

      if(failed.empty() )
         this->database->getChunksTable()->update(chunkList);

      break;
   }

   default:
      throw std::runtime_error("bad repair action");
   }
}

void ModeCheckFS::repairWrongChunkPath(std::pair<FsckChunk, FsckFileInode>& error, FsckErrCount& errCount,
   UserPrompter& prompt)
{
   errCount.fixableErrors++;

   FsckRepairAction action = prompt.chooseAction("Entry ID: " + error.first.getID()
      + "; " + (error.first.getBuddyGroupID()
         ? "Group: " + StringTk::uintToStr(error.first.getBuddyGroupID())
         : "Target: " + StringTk::uintToStr(error.first.getTargetID()))
      + "; Chunk path: " + error.first.getSavedPath()->str()
      + "; File path: "
      + this->database->getDentryTable()->getPathOf(db::EntryID::fromStr(error.first.getID() ) ) );

   switch(action)
   {
   case FsckRepairAction_UNDEFINED:
   case FsckRepairAction_NOTHING:
      break;

   case FsckRepairAction_MOVECHUNK: {
      NodeStore* nodeStore = Program::getApp()->getStorageNodes();
      TargetMapper* targetMapper = Program::getApp()->getTargetMapper();

      auto storageNode = nodeStore->referenceNode(
         targetMapper->getNodeID(error.first.getTargetID() ) );

      std::string moveToPath = DatabaseTk::calculateExpectedChunkPath(error.first.getID(),
         error.second.getPathInfo()->getOrigUID(),
         error.second.getPathInfo()->getOrigParentEntryID(),
         error.second.getPathInfo()->getFlags() );

      if(MsgHelperRepair::moveChunk(*storageNode, error.first, moveToPath, false) )
      {
         FsckChunkList chunks(1, error.first);
         this->database->getChunksTable()->update(chunks);
         break;
      }

      // chunk file exists at the correct location
      FsckTkEx::fsckOutput("Chunk file for " + error.first.getID()
         + " already exists at the correct location. ", OutputOptions_NONE);

      if(Program::getApp()->getConfig()->getAutomatic() )
      {
         FsckTkEx::fsckOutput("Will not attempt automatic repair.", OutputOptions_LINEBREAK);
         break;
      }

      char chosen = 0;

      while(chosen != 'y' && chosen != 'n')
      {
         FsckTkEx::fsckOutput("Move anyway? (y/n) ", OutputOptions_NONE);

         std::string line;
         std::getline(std::cin, line);

         if(line.size() == 1)
            chosen = line[0];
      }

      if(chosen != 'y')
         break;

      if(MsgHelperRepair::moveChunk(*storageNode, error.first, moveToPath, true) )
      {
         FsckChunkList chunks(1, error.first);
         this->database->getChunksTable()->update(chunks);
      }
      else
      {
         FsckTkEx::fsckOutput("Repair failed, see log file for more details.",
            OutputOptions_LINEBREAK);
      }

      break;
   }

   default:
      throw std::runtime_error("bad repair action");
   }
}

void ModeCheckFS::updateOldStyledHardlinks(db::FileInode& inode, FsckErrCount& errCount,
   UserPrompter& prompt)
{
   errCount.fixableErrors++;

   FsckRepairAction action = prompt.chooseAction("Entry ID:  " + inode.id.str()
      + "; link count: " + std::to_string(inode.numHardlinks));

   std::string linkName = this->database->getDentryTable()->getNameOf(inode.id);
   switch (action)
   {
   case FsckRepairAction_UNDEFINED:
   case FsckRepairAction_NOTHING:
      break;

   case FsckRepairAction_UPDATEOLDTYLEDHARDLINKS:
   {
      StringList failedEntries;
      EntryInfo entryInfo(NumNodeID(inode.saveNodeID), inode.parentDirID.str(), inode.id.str(),
         linkName, DirEntryType_REGULARFILE, 0);
      entryInfo.setInodeInlinedFlag(inode.isInlined);
      entryInfo.setBuddyMirroredFlag(inode.isBuddyMirrored);

      MsgHelperRepair::deinlineFileInode(entryInfo.getOwnerNodeID(), &entryInfo, failedEntries,
         secondariesSetBad);

      if (failedEntries.empty())
      {
         FsckFileInode fsckInode = inode.toInodeWithoutStripes();
         fsckInode.setIsInlined(false);

         fsckInode.setStripeTargets(this->database->getFileInodesTable()->getStripeTargetsByKey(inode.id));
         FsckFileInodeList fsckInodeList{fsckInode};
         this->database->getFileInodesTable()->update(fsckInodeList);
      }

      break;
   }

   default:
      throw std::runtime_error("bad repair action");
   }
}

void ModeCheckFS::deleteFsIDsFromDB(FsckDirEntryList& dentries)
{
   // create the fsID list
   FsckFsIDList fsIDs;

   for ( FsckDirEntryListIter iter = dentries.begin(); iter != dentries.end(); iter++ )
   {
      FsckFsID fsID(iter->getID(), iter->getParentDirID(), iter->getSaveNodeID(),
         iter->getSaveDevice(), iter->getSaveInode(), iter->getIsBuddyMirrored());
      fsIDs.push_back(fsID);
   }

   this->database->getFsIDsTable()->remove(fsIDs);
}

void ModeCheckFS::deleteFilesFromDB(FsckDirEntryList& dentries)
{
   for ( FsckDirEntryListIter dentryIter = dentries.begin(); dentryIter != dentries.end();
      dentryIter++ )
   {
      std::pair<bool, db::FileInode> inode = this->database->getFileInodesTable()->get(
         dentryIter->getID() );

      if(inode.first)
      {
         this->database->getFileInodesTable()->remove(inode.second.id);
         this->database->getChunksTable()->remove(inode.second.id);
      }
   }

   this->database->getDentryTable()->remove(dentries);

   // delete the dentry-by-id files
   this->deleteFsIDsFromDB(dentries);
}

bool ModeCheckFS::ensureLostAndFoundExists()
{
   this->lostAndFoundNode = MsgHelperRepair::referenceLostAndFoundOwner(&this->lostAndFoundInfo);

   if(!this->lostAndFoundNode)
   {
      if(!MsgHelperRepair::createLostAndFound(this->lostAndFoundNode, this->lostAndFoundInfo) )
         return false;
   }

   std::pair<bool, FsckDirInode> dbLaF = this->database->getDirInodesTable()->get(
      this->lostAndFoundInfo.getEntryID() );

   if(dbLaF.first)
      this->lostAndFoundInode.reset(new FsckDirInode(dbLaF.second) );

   return true;
}

void ModeCheckFS::releaseLostAndFound()
{
   if(!this->lostAndFoundNode)
      return;

   if(this->lostAndFoundInode)
   {
      FsckDirInodeList updates(1, *this->lostAndFoundInode);
      this->database->getDirInodesTable()->update(updates);
   }

   this->lostAndFoundInode.reset();
}
