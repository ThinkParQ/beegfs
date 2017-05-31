#include <app/App.h>
#include <common/net/message/storage/attribs/GetEntryInfoMsg.h>
#include <common/net/message/storage/attribs/GetEntryInfoRespMsg.h>
#include <common/toolkit/MetadataTk.h>
#include <common/toolkit/NodesTk.h>
#include <common/toolkit/UnitTk.h>
#include <common/toolkit/ZipIterator.h>
#include <program/Program.h>
#include <modes/modehelpers/ModeHelper.h>
#include <modes/modehelpers/ModeInterruptedException.h>

#include "ModeMigrate.h"
#include "MigrateFile.h"

#define MGMT_TIMEOUT_MS 2500

#define MODE_FIND_ERR_SUCCESS     0 // file is located on given target
#define MODE_FIND_ERR_INTERNAL   -1 // internal error
#define MODE_FIND_ERR_NOT_FOUND  -2 // file not found on given targets

typedef std::map<NumNodeID, UInt16List*> NodeTargetMap;
typedef NodeTargetMap::iterator NodeTargetMapIter;

/**
 * Entry method for mode "migrate" (and for mode "find" via executeFind() with
 * this->cfgFindOnly==true).
 */
int ModeMigrate::doExecute()
{
   App* app = Program::getApp();

   NodeStoreServers* mgmtNodes = app->getMgmtNodes();
   NodeHandle mgmtNode;
   int findRes = MODE_FIND_ERR_SUCCESS;

   int retVal = getParams();
   if (retVal != APPCODE_NO_ERROR)
      return retVal;

   struct stat statBuf;
   int statRes = lstat(this->cfgSearchPath.c_str(), &statBuf);
   if (statRes < 0)
   {
      std::cerr << "Failed to stat given search path: " << strerror(errno) << std::endl;
      return APPCODE_RUNTIME_ERROR;
   }

   int fileType = StorageTk::modeToDentryType(statBuf.st_mode);
   std::string fileName;
   std::string dirName;

   if (!S_ISDIR(statBuf.st_mode) )
   {
      // search path is a file, get the parent root directory first

      dirName = StorageTk::getPathDirname(this->cfgSearchPath);
      fileName = StorageTk::getPathBasename(this->cfgSearchPath);

      this->rootFD = open(dirName.c_str(), O_RDONLY | O_NOFOLLOW | O_NOCTTY);
      if (this->rootFD < 0)
      {
         std::cerr << "Failed to open directory (" << dirName << "): "
            << strerror(errno) << std::endl;
         return APPCODE_RUNTIME_ERROR;
      }
   }
   else
   {
      // search path is a directory
      this->rootFD = open(this->cfgSearchPath.c_str(), O_RDONLY | O_NOFOLLOW | O_NOCTTY);
      if (this->rootFD < 0)
      {
         std::cerr << "Failed to open search path: " << strerror(errno) << std::endl;
         return APPCODE_RUNTIME_ERROR;
      }
   }

   // for later realpath() we need to be relative to rootFD
   int chDirRes = fchdir(this->rootFD);
   if (chDirRes < 0)
   {
      std::cerr << "Failed to chdir to: " << dirName << std::endl;
      return APPCODE_RUNTIME_ERROR;
   }

   if (!ModeHelper::fdIsOnFhgfs(this->rootFD) )
   {
      std::cerr << this->cfgSearchPath << " is not a BeeGFS directory. Aborting." << std::endl;
      return APPCODE_RUNTIME_ERROR;
   }

   // block ctrl-c
   sigset_t oldMask;
   ModeHelper::blockInterruptSigs(&oldMask);

   mgmtNode = mgmtNodes->referenceFirstNode();

   retVal = getTargets(*mgmtNode);
   if (retVal != APPCODE_NO_ERROR)
      goto out;


   /* Note: Don't try to use ModeHelper::registerInterruptSignalHandler() and try catch
    *       ModeInterruptedException here, it will not work properly and SIGABRT traces will
    *       be shown. In general it does not work properly to throw exceptions from the
    *       signal-handler. But especially with our processDir recursion it fails at all here. */
   findRes = findFiles(fileName, dirName, fileType);
   if(findRes == MODE_FIND_ERR_NOT_FOUND)
   {
      std::cerr << "Given file is not located on the given target." << std::endl;
      retVal = APPCODE_RUNTIME_ERROR;
   }
   else
   if(findRes == MODE_FIND_ERR_INTERNAL)
   {
      std::cerr << "Aborting after unrecoverable error." << std::endl;
      retVal = APPCODE_RUNTIME_ERROR;
   }

out:
   // cleanup

   ModeHelper::restoreSigs(&oldMask);

   close(this->rootFD);

   return retVal;
}


/**
 * Map all targets - so far those are storage Targets only, until our meta server also uses targets
 */
int ModeMigrate::getTargets(Node& mgmtNode)
{
   // get lists of targetIDs and nodeIDs, mapped 1:1
   UInt16List targetIDs;
   NumNodeIDList nodeIDs;
   if(!NodesTk::downloadTargetMappings(mgmtNode, &targetIDs, &nodeIDs, false) )
   {
      std::cerr << "Mappings download failed." << std::endl;
      return APPCODE_RUNTIME_ERROR;
   }

   if (targetIDs.size() != nodeIDs.size() )
   {
      std::cerr << "Bug: targetIDs.size() vs. nodeIDs.size() mismatch." << std::endl;
      return APPCODE_RUNTIME_ERROR;
   }

   int fromRes = getFromTargets(targetIDs, nodeIDs);
   if (fromRes != APPCODE_NO_ERROR)
      return fromRes;

   int destRes = getDestTargets(targetIDs, nodeIDs);
   if (destRes != APPCODE_NO_ERROR)
      return destRes;

   // get MirrorBuddyGroups
   UInt16List mirrorBuddyGroupIDs;
   UInt16List mirrorBuddyGroupPrimaryTargetIDs;
   UInt16List mirrorBuddyGroupSecondaryTargetIDs;
   if(!NodesTk::downloadMirrorBuddyGroups(mgmtNode, NODETYPE_Storage, &mirrorBuddyGroupIDs,
      &mirrorBuddyGroupPrimaryTargetIDs, &mirrorBuddyGroupSecondaryTargetIDs, false) )
   {
      std::cerr << "Download of mirror buddy groups failed." << std::endl;
      return APPCODE_RUNTIME_ERROR;
   }

   if ( (mirrorBuddyGroupIDs.size() != mirrorBuddyGroupPrimaryTargetIDs.size() ) ||
      (mirrorBuddyGroupIDs.size() != mirrorBuddyGroupSecondaryTargetIDs.size() ) )
   {
      std::cerr << "Bug: mirrorBuddyGroupIDs.size() vs. mirrorBuddyGroupPrimaryTargetIDs.size() " <<
         "vs. mirrorBuddyGroupSecondaryTargetIDs.size() mismatch." << std::endl;
      return APPCODE_RUNTIME_ERROR;
   }

   fromRes = getFromAndDestMirrorBuddyGroups(mirrorBuddyGroupIDs, mirrorBuddyGroupPrimaryTargetIDs,
      mirrorBuddyGroupSecondaryTargetIDs);
   if (fromRes != APPCODE_NO_ERROR)
      return fromRes;

   return APPCODE_NO_ERROR;
}

/**
 * Get the targets we want to migrate files to
 */
int ModeMigrate::getDestTargets(UInt16List& targetIDs, NumNodeIDList& nodeIDs)
{
   ZipIterRange<UInt16List, NumNodeIDList> targetNodeIDIt(targetIDs, nodeIDs);
   NodeTargetMap targetMap;

   /* Walk over the 1:1 mapped targetIDs and nodeIDs here and put targets in a per node list
    * Those lists can be located by the node-key in a map */
   while (!targetNodeIDIt.empty() )
   {
      bool isSearchTarget = false; // search targets are those we want to migrate files from

      uint16_t targetID = *targetNodeIDIt()->first;
      NumNodeID nodeID = *targetNodeIDIt()->second;

      // check the targets to migrate files from
      UInt16ListIter searchTargetIter = this->searchStorageTargetIDs.begin();
      while (searchTargetIter != this->searchStorageTargetIDs.end() )
      {
         if (*searchTargetIter == targetID)
         {
            isSearchTarget = true;
            break;
         }
         searchTargetIter++;
      }

      if (!isSearchTarget)
      {
         NodeTargetMapIter mapIter = targetMap.find(nodeID);
         // add target to per node target maps
         if ( mapIter == targetMap.end() )
         {
            UInt16List* newStringList = new UInt16List;
            targetMap.insert(NodeTargetMap::value_type(nodeID, newStringList));
            mapIter = targetMap.find(nodeID);
         }

         UInt16List* perNodeTargetList = mapIter->second;
         perNodeTargetList->push_back(targetID);

      }

      ++targetNodeIDIt;
   }

   /* We have the targets in lists per node. Now we will add those targets to a linear list,
    * but will distribute over the nodes. So we take a target from
    * node1, then from node, ..., nodeN and then will start at node1 again.
    */
   while (!targetMap.empty() )
   {
      NodeTargetMapIter mapIter = targetMap.begin();
      while (mapIter != targetMap.end())
      {
         UInt16List* perNodeTargetList = mapIter->second;
         uint16_t target = perNodeTargetList->front();

         this->destStorageTargetIDs.push_back(target);
         perNodeTargetList->pop_front();

         NodeTargetMapIter oldMapIter = mapIter;
         mapIter++; // next node in the map
         if (perNodeTargetList->empty())
         {
            // this node has no remaining targets anymore, remove it from the map
            delete perNodeTargetList;
            targetMap.erase(oldMapIter);
         }
      }
   }

   if (this->destStorageTargetIDs.empty() && !this->cfgFindOnly)
   {
      std::cerr << "Error: Could not find any storage targets to write files to. Aborting."
         << std::endl;
      return APPCODE_RUNTIME_ERROR;
   }

   return APPCODE_NO_ERROR;
}


/**
 * Get the targets we want migrate files off. The result will be saved into
 * this->searchStorageTargetIDs
 *
 * @return APPCODE_...
 */
int ModeMigrate::getFromTargets(UInt16List& targetIDs, NumNodeIDList& nodeIDs)
{
   if (this->cfgTargetID)
   { // we are looking for a specific targetID
      this->searchStorageTargetIDs.push_back(cfgTargetID);

      return APPCODE_NO_ERROR;
   }


   ZipIterRange<UInt16List, NumNodeIDList> targetNodeIter(targetIDs, nodeIDs);

   if (this->cfgNodeID != 0)
   { // get all targetIDs from the given node

      while (!targetNodeIter.empty() )
      {
         if (*(targetNodeIter()->second) == cfgNodeID)
            this->searchStorageTargetIDs.push_back(*(targetNodeIter()->first));

         ++targetNodeIter;
      }

      return APPCODE_NO_ERROR;
   }


   // we never should get here!
   std::cerr << "Bug: Unexpected end of function: " << __func__ << std::endl;
   return APPCODE_RUNTIME_ERROR;
}


int ModeMigrate::getFromAndDestMirrorBuddyGroups(UInt16List& mirrorBuddyGroupIDs,
   UInt16List& buddyGroupPrimaryTargetIDs, UInt16List& buddyGroupSecondaryTargetIDs)
{
   UInt16List destGroupIDs;

   for (ZipIterRange<UInt16List, UInt16List, UInt16List>
        iter(mirrorBuddyGroupIDs, buddyGroupPrimaryTargetIDs, buddyGroupSecondaryTargetIDs);
        !iter.empty(); ++iter)
   {
      bool targetFound = false;

      UInt16ListIter searchTargetIter = this->searchStorageTargetIDs.begin();
      while (searchTargetIter != this->searchStorageTargetIDs.end() )
      {
         if( (*(iter()->second) == *searchTargetIter) ||
            (*(iter()->third) == *searchTargetIter) )
         {
            this->searchMirrorBuddyGroupIDs.push_back(*(iter()->first));
            targetFound = true;
            break;
         }

         searchTargetIter++;
      }

      if(!targetFound && !mirrorBuddyGroupIDs.empty() )
         destGroupIDs.push_back(*(iter()->first));
   }

   this->searchMirrorBuddyGroupIDs.unique();
   destGroupIDs.unique();
   this->destMirrorBuddyGroupIDs = UInt16Vector(destGroupIDs.begin(), destGroupIDs.end() );

   return APPCODE_NO_ERROR;
}

/**
 * Get and check program arguments
 *
 * @return APPCODE_...
 */
int ModeMigrate::getParams()
{
   StringMapIter iter;

   // nodeType
   bool nodeTypeWasSet;
   NodeType nodeType = ModeHelper::nodeTypeFromCfg(cfg, &nodeTypeWasSet);

   if(nodeTypeWasSet)
   {
      this->cfgNodeType = nodeType;

      if ( (nodeType != NODETYPE_Meta) && (nodeType != NODETYPE_Storage) )
      {
         std::cerr << "Invalid node type set." << std::endl;
         return APPCODE_INVALID_CONFIG;
      }
   }

   // for migration, nodetype==meta is not supported yet
   if( (this->cfgNodeType == NODETYPE_Meta) && !this->cfgFindOnly)
   {
      std::cerr << "Migration from metadata nodes not supported yet." << std::endl;
      return APPCODE_INVALID_CONFIG;
   }


   // targetid
   iter = cfg->find(MODEMIGRATE_ARG_TARGETID);
   if (iter != cfg->end() )
   {
      bool isNumericRes = StringTk::isNumeric(iter->second);
      if(!isNumericRes)
      {
         std::cerr << "Invalid targetID given (must be numeric): " << iter->second << std::endl;
         return APPCODE_INVALID_CONFIG;
      }

      this->cfgTargetID = StringTk::strToUInt(iter->second);
      cfg->erase(iter);
   }

   // nodeID
   iter = cfg->find(MODEMIGRATE_ARG_NODEID);
   if (iter != cfg->end() )
   {
      bool isNumericRes = StringTk::isNumeric(iter->second);
      if(!isNumericRes)
      {
         std::cerr << "Invalid nodeID given (must be numeric): " << iter->second << std::endl;
         return APPCODE_INVALID_CONFIG;
      }

      this->cfgNodeID = NumNodeID(StringTk::strToUInt(iter->second) );
      cfg->erase(iter);
   }

   // noMirrors
   iter = cfg->find(MODEMIGRATE_ARG_NOMIRRORS);
   if (iter != cfg->end() )
   {
      this->cfgNoMirrors = true;
      cfg->erase(iter);
   }

   // verbose
   iter = cfg->find(MODEMIGRATE_ARG_VERBOSE);
   if (iter != cfg->end() )
   {
      this->cfgVerbose = true;
      cfg->erase(iter);
   }

   // path
   if(cfg->empty() )
   {
      std::cerr << "No path specified." << std::endl;
      return APPCODE_INVALID_CONFIG;
   }

   this->cfgSearchPath = cfg->begin()->first;
   cfg->erase(cfg->begin() );

   // sanity checks
   if (!this->cfgNodeID && !this->cfgTargetID)
   {
      std::cerr << "Either 'nodeid' or 'targetid' must be specified." << std::endl;
      return APPCODE_RUNTIME_ERROR;
   }

   if ((this->cfgNodeID != 0) && (this->cfgTargetID != 0))
   {
      std::cerr << "Only one of 'nodeid' or 'targetid' can be specified, not both." << std::endl;
      return APPCODE_RUNTIME_ERROR;
   }

   // for metadata the targetID and the nodeID is the same ID, so set the nodeID if the targetID is
   // given, because only the nodeID is used for metadata
   if ( (this->cfgNodeType == NODETYPE_Meta) && !this->cfgNodeID)
      this->cfgNodeID = NumNodeID(this->cfgTargetID);

   if(ModeHelper::checkInvalidArgs(cfg) )
      return APPCODE_INVALID_CONFIG;

   return APPCODE_NO_ERROR;
}

/**
 * Print help for mode "migrate"
 */
void ModeMigrate::printHelpMigrate()
{
   // note: if you make changes here, you might also want to update printHelpFind()

   std::cout << "MODE ARGUMENTS:" << std::endl;
   std::cout << " Mandatory:" << std::endl;
   std::cout << "  <path>                 Path to a file or directory." << std::endl;
   std::cout << std::endl;

   // in migrate mode
   std::cout << " Optional:" << std::endl;
   std::cout << "  --targetid=<targetID>  Migrate files away from the given targetID." << std::endl;
   std::cout << "  --nodeid=<nodeID>      Migrate files away from all targets attached" << std::endl;
   std::cout << "                         to this nodeID." << std::endl;
   std::cout << "  --nomirrors            Migrate only files which are not buddy mirrored." << std::endl;
   std::cout << "  --verbose              Print verbose messages, e.g. all files being migrated." << std::endl;

   std::cout << std::endl;
   std::cout << " Note: Either targetID or nodeID must be specified." << std::endl;
   std::cout << std::endl;

   std::cout << "USAGE:" << std::endl;
   std::cout << " This mode frees up storage targets by moving the contained files to other" << std::endl;
   std::cout << " storage targets." << std::endl;
   std::cout << " To achieve this, each matching file will be copied to a temporary file on a" << std::endl;
   std::cout << " different set of storage targets and then be renamed to the orignal file name." << std::endl;
   std::cout << " Thus, migration should not be done while applications are modifiying files in" << std::endl;
   std::cout << " the migration directory tree." << std::endl;
   std::cout << " If new files are created by applications while migration is in progress, it" << std::endl;
   std::cout << " is possible that these files are created on the target which you want to free" << std::endl;
   std::cout << " up. Thus, be careful regarding how you access the file system during migration." << std::endl;
   std::cout << std::endl;
   std::cout << " Example: Migrate all files from storage target with ID \"5\" to other targets." << std::endl;
   std::cout << "  $ beegfs-ctl --migrate --targetid=5 /mnt/beegfs" << std::endl;
}

/**
 * Print help for mode "find"
 */
void ModeMigrate::printHelpFind()
{
   // note: if you make changes here, you might also want to update printHelpMigrate()

   std::cout << "MODE ARGUMENTS:" << std::endl;
   std::cout << " Mandatory:" << std::endl;
   std::cout << "  <path>                 Path to a file or directory." << std::endl;
   std::cout << std::endl;

   std::cout << " Optional:" << std::endl;
   std::cout << "  --targetid=<targetID>  Find files on the given targetID." << std::endl;
   std::cout << "  --nodeid=<nodeID>      Find files on targets attached to this nodeID." << std::endl;
   std::cout << "  --nodetype=<nodetype>  Find files on the given nodetype (meta, storage)." << std::endl;
   std::cout << "                         (Default: storage)" << std::endl;
   std::cout << "  --nomirrors            Do not include buddy mirrored files in search." << std::endl;

   std::cout << std::endl;
   std::cout << " Note: Either targetID or nodeID must be specified." << std::endl;
   std::cout << std::endl;

   std::cout << "USAGE:" << std::endl;
   std::cout << " This mode finds files, which are located on certain servers or storage" << std::endl;
   std::cout << " targets." << std::endl;
   std::cout << std::endl;
   std::cout << " Example: Find all files that have chunks on storage target with ID \"5\"." << std::endl;
   std::cout << "  $ beegfs-ctl --find --targetid=5 /mnt/beegfs" << std::endl;
}


/**
 * Find files the user is looking for
 *
 * @param fileName  - last elelement (basename) of the path given by the user
 *                    will be empty ("") if the user gave a path
 * @param dirName   - name of the parent directory, only set if the user specified a file
 * @param fileType  - fileTypes as in readdir()s dentry->d_type
 * @return 0 file is located on given target, -1 internal error, -2 file not found on given targets
 */
int ModeMigrate::findFiles(std::string fileName, std::string dirName, int fileType)
{
   int retVal = MODE_FIND_ERR_SUCCESS;

   if (fileType == DT_DIR)
   {
      std::string rootPath = ""; // relative to this->rootFD
      if(!processDir(rootPath) )
         retVal = MODE_FIND_ERR_INTERNAL;
   }
   else
   { // some kind of file
      bool isDir = false;
      bool isBuddyMirrored = false;
      unsigned numTargets;

      int testRes = testFile(this->cfgSearchPath, isDir, &numTargets, &isBuddyMirrored);

      if (testRes == MODE_FIND_ERR_SUCCESS)
      { // file matches given targetID/nodeID
         EntryInfo *entryInfo = NULL;

         bool migrateRes = startFileMigration(fileName, fileType, dirName, this->rootFD,
            numTargets, &entryInfo, isBuddyMirrored);

         // (note: we ignore migration errors so far. maybe add option to abort after n errors?)
         if (!migrateRes)
            std::cerr << "Failed to migrate file: " << dirName + '/' + fileName << std::endl;
         else
         if (this->cfgVerbose && !this->cfgFindOnly)
            std::cout << "Successfully migrated: " << dirName + '/' + fileName << std::endl;

         SAFE_DELETE(entryInfo);
      }
      else
         retVal = testRes;

   }

   return retVal;
}

/**
 * Recursively walk through directories, files are checked
 *
 *@param dirPath  - The path we want to read entries in, *relative* to this->rootFD, will be
 *                  an empty string ("") if it is the root. Root here means the
 *                  directory given by the user. It also also important to use relative
 *                  path, as openat(rootFD, ...) *requires* relative paths and falls back to
 *                  open() for absolute paths.
 */
bool ModeMigrate::processDir(std::string& dirPath)
{
   bool retVal = true;
   EntryInfo *entryInfo = NULL;

   if (ModeHelper::isInterruptPending() )
      return true; // user wants to stop the program

   // recursively read the directory
   int dirFD;
   if (dirPath.length() > 0)
   {
      dirFD = openat(this->rootFD, dirPath.c_str(), O_RDONLY);
      if (dirFD < 0)
      {
         std::cerr << "Failed to open directory (" << dirPath << ") "
            << System::getErrString() << std::endl;
         return false;
      }
   }
   else
      dirFD = dup(this->rootFD); // first call of this function when user gave a directory


   if (!ModeHelper::fdIsOnFhgfs(dirFD) )
   {
      /* sub directory is not on FhGFS, silently ignore it, but hrmm, why don't
       * openat() and fdopendir() do not protect us from that? */
      close(dirFD);
      return true;
   }

   DIR* dir = fdopendir(dirFD);
   if (!dir)
   {
      std::cerr << "Failed to open directory (" << dirPath << "): "
         << System::getErrString() << std::endl;
      return true;
   }

   struct dirent* dentry = StorageTk::readdirFiltered(dir);
   while (dentry && !ModeHelper::isInterruptPending() )
   {
      SAFE_DELETE(entryInfo);

      std::string newPath;
      if (dirPath.length() == 0)
         newPath = dentry->d_name; // we must not prepend a "/", as openat() needs a relative path!
      else
      {
         // files in sub-directories have to be separated by a "/", of course
         newPath = dirPath + "/";
         newPath.append(dentry->d_name);
      }

      if(dentry->d_type == DT_DIR)
      {  // a directory, recurse into it and continue there

         // bool isDir = true;
         // testFile(newPath, isDir, NULL); // TODO: Only required once we support metadata
         /* we ignore errors here, as the user just might have deleted a file, while we were
          * processing the directory */

         bool procDirRes = this->processDir(newPath);
         if (!procDirRes)
         {
            std::cerr << "Skipping dir: " << newPath << std::endl;
            dentry = StorageTk::readdirFiltered(dir);
            continue;
         }
      }
      else
      {  // a single file only
         bool isDir = false;
         bool isBuddyMirrored = false;
         unsigned numTargets;
         int testRes = testFile(newPath, isDir, &numTargets, &isBuddyMirrored);
         if (testRes == MODE_FIND_ERR_SUCCESS)
         {
            /* getentryInfo would fail for "" if we are still in our search root
             * and didn't request it before
             */
            std::string path = dirPath.length() ? dirPath : ".";

            bool migrateRes = startFileMigration(dentry->d_name, dentry->d_type, path, dirFD,
               numTargets, &entryInfo, isBuddyMirrored);

            // (note: we ignore migration errors so far. maybe add option to abort after n errors?)
            if (!migrateRes)
               std::cerr << "Failed to migrate file: " << newPath << std::endl;
            else
            if (this->cfgVerbose && !this->cfgFindOnly)
               std::cout << "Successfully migrated: " << newPath << std::endl;
         }
      }

      dentry = StorageTk::readdirFiltered(dir);
   }

   SAFE_DELETE(entryInfo);
   closedir(dir);
   close(dirFD);

   return retVal;
}

/**
 * So a file was positively tested, initiate migration or print.
 */
bool ModeMigrate::startFileMigration(std::string fileName, int fileType, std::string path,
   int dirFD, int numTargets, EntryInfo **inOutEntryInfo, bool isBuddyMirrored)
{
   if (this->cfgFindOnly)
   {
      // we only need to print the files without any migration (used by ModeFind)

      std::cout << path + "/" + fileName << std::endl;

      return true;
   }


   // so we need to migrate the file

   // first block all signals, to make sure files are not only partly migrated
   sigset_t oldMask;
   ModeHelper::blockAllSigs(&oldMask);

   if (!*inOutEntryInfo)
   {
      /* parent information, we only request it once per directory and only if we
       * should need it */
      *inOutEntryInfo = getEntryInfo(path);

      if (!*inOutEntryInfo)
      {
         std::cerr << "Failed to get parent EntryInfo. Ignoring: " << path + "/" + fileName <<
            std::endl;
         return false;
      }
   }

   UInt16Vector* destTargets;
   if(isBuddyMirrored)
      destTargets = &this->destMirrorBuddyGroupIDs;
   else
      destTargets = &this->destStorageTargetIDs;

   if(destTargets->empty() )
   {
      std::string errorTargetString;
      if(isBuddyMirrored)
         errorTargetString = "targets";
      else
         errorTargetString = "MirrorBuddyGroups";

      std::cerr << "Can not migrate file. No destination " << errorTargetString <<
         " available. Ignoring: " << path + "/" + fileName << std::endl;
      return false;
   }

   struct MigrateDirInfo dirInfo(path, dirFD, *inOutEntryInfo);
   struct MigrateFileInfo fileInfo(fileName, fileType, numTargets, destTargets);
   MigrateFile migrateFile(&dirInfo, &fileInfo);

   // eventually do the migration
   bool retVal = migrateFile.doMigrate();


   ModeHelper::restoreSigs(&oldMask);

   return retVal;
}

/**
 * Test if the file given by 'path' has chunks on the given cfgTargetID or metadata on the given
 * cfgNodeID.
 *
 * @param path                the path to test
 * @param cfgTargetID         cfgTargetID the user wants to test
 * @param isDir               is path a directory?
 * @param outNumTargets       out value for the number of desired targets
 * @param outIsBuddyMirrored  out value, true if the path is BuddyMirrored
 * @return 0 file is located on given target, -1 internal error, -2 file not found on given targets
 */
int ModeMigrate::testFile(std::string& path, bool isDir, unsigned* outNumTargets,
   bool* outIsBuddyMirrored)
{
   int retVal = MODE_FIND_ERR_SUCCESS;

   StripePattern* stripePattern = NULL;
   NumNodeID metaOwnerNodeID;
   EntryInfo entryInfo;

   bool entryRes = getEntryTargets(path, &stripePattern, &metaOwnerNodeID, entryInfo);
   if (!entryRes)
   {
      std::cerr << "Getting entry information failed for path: " << path << std::endl;
      retVal = MODE_FIND_ERR_INTERNAL;
      goto noEntryInfo;
   }

   *outIsBuddyMirrored =
         (cfgNodeType == NODETYPE_Storage
            && stripePattern->getPatternType() == StripePatternType_BuddyMirror)
         || (cfgNodeType == NODETYPE_Meta && entryInfo.getIsBuddyMirrored());

   if (this->cfgNodeType == NODETYPE_Invalid)
   {
      if (metaOwnerNodeID != this->cfgNodeID)
         retVal = MODE_FIND_ERR_NOT_FOUND;
   }

   if (cfgNodeType == NODETYPE_Meta)
   {
      if (entryInfo.getIsBuddyMirrored())
      {
         if (cfgNoMirrors)
            retVal = MODE_FIND_ERR_NOT_FOUND;
         else
         {
            auto* bgm = Program::getApp()->getMetaMirrorBuddyGroupMapper();
            auto group = bgm->getMirrorBuddyGroup(entryInfo.getOwnerNodeID().val());

            if (group.firstTargetID != cfgNodeID.val() && group.secondTargetID != cfgNodeID.val())
               retVal = MODE_FIND_ERR_NOT_FOUND;
         }
      }
      else
      {
         if (metaOwnerNodeID != cfgNodeID)
            retVal = MODE_FIND_ERR_NOT_FOUND;
      }
   }

   if( (retVal == MODE_FIND_ERR_SUCCESS) && !isDir &&
      (this->cfgNodeType == NODETYPE_Storage || this->cfgNodeType == NODETYPE_Invalid) )
   {
      if(!checkFileStripes(stripePattern) )
         retVal = MODE_FIND_ERR_NOT_FOUND;
   }

   if (retVal == MODE_FIND_ERR_SUCCESS && cfgNodeType == NODETYPE_Meta
         && entryInfo.getIsBuddyMirrored() && cfgNoMirrors)
      retVal = MODE_FIND_ERR_NOT_FOUND;

   *outNumTargets = stripePattern->getDefaultNumTargets();
   delete stripePattern;

noEntryInfo:
   return retVal;
}

/**
 * @return EntryInfo object must be deleted by caller
 */
EntryInfo* ModeMigrate::getEntryInfo(std::string& path)
{
   NodeHandle metaOwnerNode;

   EntryInfo *outEntryInfo = new EntryInfo();
   if (!outEntryInfo)
   {
      std::cerr << "Failed to allocate memory (" << __func__ <<")." << std::endl;
      return NULL;
   }

   int res = pathToEntryInfo(path, outEntryInfo, metaOwnerNode);
   if (res != APPCODE_NO_ERROR)
   {
      delete(outEntryInfo);
      return NULL;
   }

   return outEntryInfo;
}

/**
 * Gets all information about the file (given by path) and allocates outStripePattern.
 *
 * @param outStripePattern must be deleted by the caller
 */
bool ModeMigrate::getEntryTargets(std::string& path, StripePattern** outStripePattern,
   NumNodeID* outMetaOwnerNodeID, EntryInfo& pathInfo)
{
   bool retVal = false;

   NodeHandle metaOwnerNode;

   bool commRes;
   char* respBuf = NULL;
   NetMessage* respMsg = NULL;
   GetEntryInfoRespMsg* respMsgCast;

   FhgfsOpsErr getInfoRes;

   int res = pathToEntryInfo(path, &pathInfo, metaOwnerNode);
   if (res != APPCODE_NO_ERROR)
      return false;

   GetEntryInfoMsg getInfoMsg(&pathInfo);

   // request/response
   commRes = MessagingTk::requestResponse(
      *metaOwnerNode, &getInfoMsg, NETMSGTYPE_GetEntryInfoResp, &respBuf, &respMsg);
   if(!commRes)
      goto err_cleanup;

   respMsgCast = (GetEntryInfoRespMsg*)respMsg;

   getInfoRes = (FhgfsOpsErr)respMsgCast->getResult();
   if(getInfoRes != FhgfsOpsErr_SUCCESS)
   {
      std::cerr << "Server encountered an error: " << FhgfsOpsErrTk::toErrString(getInfoRes) <<
         std::endl;
      retVal = false;
      goto err_cleanup;
   }

   *outStripePattern = respMsgCast->getPattern().clone();
   *outMetaOwnerNodeID = metaOwnerNode->getNumID();

   retVal = true;

err_cleanup:
   SAFE_DELETE(respMsg);
   SAFE_FREE(respBuf);

   return retVal;
}

/**
 * convert a path (file) to the filesystem unique ID
 */
int ModeMigrate::pathToEntryInfo(std::string& pathStr, EntryInfo* outEntryInfo,
   NodeHandle& outMetaOwnerNode)
{
   std::string relPath; // relative path
   static std::string mountRoot; // NOTE: The result will be re-used after the first call!

   bool relRes = ModeHelper::makePathRelativeToMount(pathStr, false, false, &mountRoot, &relPath);
   if(!relRes)
      return APPCODE_RUNTIME_ERROR;

   Path path("/" + relPath);

   App* app = Program::getApp();
   NodeStoreServers* metaNodes = app->getMetaNodes();
   MirrorBuddyGroupMapper* metaBuddyGroupMapper = app->getMetaMirrorBuddyGroupMapper();

   FhgfsOpsErr findRes = MetadataTk::referenceOwner(&path, false, metaNodes, outMetaOwnerNode,
      outEntryInfo, metaBuddyGroupMapper);

   if(findRes != FhgfsOpsErr_SUCCESS)
   {
      std::cerr << "Unable to find metadata node for path: " << pathStr << std::endl;
      std::cerr << "Error: " << FhgfsOpsErrTk::toErrString(findRes) << std::endl;

      return APPCODE_RUNTIME_ERROR;
   }

   return APPCODE_NO_ERROR;
}

/**
 * Check if the file we are currently processing matches the target the user is asking for.
 */
bool ModeMigrate::checkFileStripes(StripePattern* stripePattern)
{
   UInt16List* searchIDs;

   const UInt16Vector& fileTargetIDs = *stripePattern->getStripeTargetIDs();

   // handle storage BuddyMirrorGroups
   if (stripePattern->getPatternType() == StripePatternType_BuddyMirror)
   {
      if(this->cfgNoMirrors)
         return false;
      else // switch searchIDs to MirrorBuddyGroupIDs
         searchIDs = &this->searchMirrorBuddyGroupIDs;
   }
   else
      searchIDs = &this->searchStorageTargetIDs;

   if(unlikely(fileTargetIDs.empty() ) )
   { // should never happen: file has not a single target assigned
      return false;
   }

   UInt16VectorConstIter fileTargetIter = fileTargetIDs.begin();
   while (fileTargetIter != fileTargetIDs.end() )
   {

      UInt16ListIter checkTargetIter = searchIDs->begin();
      while (checkTargetIter != searchIDs->end() )
      {
         // here we compare the file stripe targets with the target(s) that the user has given.
         // Reminder: searchStorageTargetIDs has several entries, if the user specified a node name
         // that has several targets.
         if (*fileTargetIter == *checkTargetIter)
            return true;

         checkTargetIter++;
      }

      fileTargetIter++;
   }

   return false;
}

