#ifndef MODEMIGRATE_H_
#define MODEMIGRATE_H_

/*
 * Find certain types of files. The user specifies which file to find (e.g. given by the storage
 * cfgTargetID), we walk through the local (client fhgfs-mounted) filesystem, find all files and
 * then check by querying meta information of the file matches.
 */


#include <program/Program.h>
#include <common/storage/striping/StripePattern.h>
#include <common/Common.h>
#include <modes/Mode.h>
#include <common/toolkit/MetadataTk.h>
#include <common/storage/EntryInfo.h>


#define MODEMIGRATE_ARG_TARGETID      "--targetid"
#define MODEMIGRATE_ARG_NODEID        "--nodeid"
#define MODEMIGRATE_ARG_STORAGEPOOLID "--storagepoolid"
#define MODEMIGRATE_ARG_DESTPOOLID    "--destinationpool"
#define MODEMIGRATE_ARG_NOMIRRORS     "--nomirrors"
#define MODEMIGRATE_ARG_VERBOSE       "--verbose"


class ModeMigrate : public Mode
{
   public:
      ModeMigrate()
      {
         App* app = Program::getApp();

         cfgTargetID = 0;
         cfgNodeType = NODETYPE_Storage; /* "invalid" (if given by user) has special meaning:
                                                  storage+meta nodes */
         cfgNoMirrors = false;
         cfgVerbose = false;
         cfgFindOnly = false;
         cfgDestStoragePoolId = StoragePoolStore::DEFAULT_POOL_ID;

         rootFD = -1;

         cfg = app->getConfig()->getUnknownConfigArgs();
      }

      virtual int execute()
      {
         return doExecute();
      }

      int executeFind()
      {
         cfgFindOnly = true;

         return doExecute();
      }

      int doExecute();

      static void printHelpMigrate();
      static void printHelpFind();


   private:

      StringMap* cfg;
      uint16_t cfgTargetID;               // the targetID we want to find
      NumNodeID cfgNodeID;                // if given, all targets from this node
      StoragePoolId cfgStoragePoolId;     // if given, all targets in this storage pool id
      StoragePoolId cfgDestStoragePoolId; // storage pool ID to use as migration destination
                                          // (defaults to the default pool Id)
      std::string cfgSearchPath;          // where to search for files
      NodeType cfgNodeType;               // meta or storage
      bool cfgNoMirrors;                  // migrate only unmirrored files
      bool cfgVerbose;                    // print verbose messages

      bool cfgFindOnly; // not a real user cfg option, set by ModeFind (i.e. find-only mode)

      NodeStoreServers* nodeStoreMeta;
      UInt16List searchStorageTargetIDs; // storage targets we are looking for and to migrate from
      UInt16Vector destStorageTargetIDs; // storage targets we will migrate data to

      UInt16List searchMirrorBuddyGroupIDs; // MBG IDs we are looking for and to migrate from
      UInt16Vector destMirrorBuddyGroupIDs; // MBG IDs we will migrate data to

      int rootFD; // file descriptor of the search path given by the user

      int getTargets(const Node& mgmtNode, const StoragePoolPtr& destStoragePool);
      int getDestTargets(const StoragePoolPtr& destStoragePool, const UInt16List& targetIDs,
         const NumNodeIDList& nodeIDs);
      int getFromTargets(UInt16List& targetIDs, NumNodeIDList& nodeIDs);
      int getFromAndDestMirrorBuddyGroups(UInt16List& mirrorBuddyGroupIDs,
         UInt16List& buddyGroupPrimaryTargetIDs, UInt16List& buddyGroupSecondaryTargetIDs);
      bool getAndCheckEntryInfo(Node& ownerNode, std::string entryID);
      int getParams();
      int findFiles(std::string fileName, std::string dirName, int fileType);
      bool processDir(std::string& path);
      bool startFileMigration(std::string fileName, int fileType, std::string path,
         int dirFD, int numTargets, EntryInfo **inOutEntryInfo, bool isBuddyMirrored);
      int testFile(std::string& path, bool isDir, unsigned* outNumTargets,
         bool* outIsBuddyMirrored);
      bool getEntryTargets(std::string& path, StripePattern** outStripePattern,
         NumNodeID* outMetaOwnerNodeID, EntryInfo& pathInfo);
      int pathToEntryInfo(std::string& pathStr, EntryInfo* outEntryInfo,
         NodeHandle& outMetaOwnerNode);
      bool checkFileStripes(StripePattern* stripePattern);
      EntryInfo* getEntryInfo(std::string& path);
};

#endif /*MODEMIGRATE_H_*/
