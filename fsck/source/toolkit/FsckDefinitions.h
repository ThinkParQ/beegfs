#ifndef FSCKDBTYPES_H_
#define FSCKDBTYPES_H_

#include <common/Common.h>
#include <common/storage/striping/StripePattern.h>
#include <common/storage/StorageDefinitions.h>

#define TABLE_NAME_DIR_ENTRIES "dirEntries"
#define TABLE_NAME_FILE_INODES "fileInodes"
#define TABLE_NAME_DIR_INODES "dirInodes"
#define TABLE_NAME_CHUNKS "chunks"
#define TABLE_NAME_STRIPE_PATTERNS "stripePatterns"
#define TABLE_NAME_CONT_DIRS "contDirs"
#define TABLE_NAME_FSIDS "fsIDs"
#define TABLE_NAME_USED_TARGETS "usedTargets"

#define TABLE_NAME_MODIFICATION_EVENTS "modificationEvents"

enum FsckRepairAction
{
   FsckRepairAction_DELETEDENTRY = 0,
   FsckRepairAction_DELETEFILE = 1,
   FsckRepairAction_CREATEDEFAULTDIRINODE = 2,
   FsckRepairAction_CORRECTOWNER = 3,
   FsckRepairAction_LOSTANDFOUND = 4,
   FsckRepairAction_CREATECONTDIR = 5,
   FsckRepairAction_DELETEINODE = 7,
   FsckRepairAction_DELETECHUNK = 8,
   FsckRepairAction_DELETECONTDIR = 9,
   FsckRepairAction_UPDATEATTRIBS = 10,
   FsckRepairAction_CHANGETARGET = 11,
   FsckRepairAction_RECREATEFSID = 12,
   FsckRepairAction_RECREATEDENTRY = 13,
   FsckRepairAction_FIXPERMISSIONS = 14,
   FsckRepairAction_MOVECHUNK = 15,
   FsckRepairAction_REPAIRDUPLICATEINODE = 16,
   FsckRepairAction_UPDATEOLDTYLEDHARDLINKS = 17,
   FsckRepairAction_NOTHING = 18,
   FsckRepairAction_UNDEFINED = 19
};

struct FsckRepairActionElem
{
   const char* actionShortDesc;
   const char* actionDesc;
   enum FsckRepairAction action;
};

// Note: Keep in sync with enum FsckErrorCode
FsckRepairActionElem const __FsckRepairActions[] =
{
      {"DeleteDentry", "Delete directory entry", FsckRepairAction_DELETEDENTRY},
      {"DeleteFile", "Delete file", FsckRepairAction_DELETEFILE},
      {"CreateDefDirInode", "Create a directory inode with default values",
         FsckRepairAction_CREATEDEFAULTDIRINODE},
      {"CorrectOwner", "Correct owner node", FsckRepairAction_CORRECTOWNER},
      {"LostAndFound", "Link to lost+found", FsckRepairAction_LOSTANDFOUND},
      {"CreateContDir", "Create an empty content directory", FsckRepairAction_CREATECONTDIR},
      {"DeleteInode", "Delete inodes", FsckRepairAction_DELETEINODE},
      {"DeleteChunk", "Delete chunk", FsckRepairAction_DELETECHUNK},
      {"DeleteContDir", "Delete content directory", FsckRepairAction_DELETECONTDIR},
      {"UpdateAttribs", "Update attributes", FsckRepairAction_UPDATEATTRIBS},
      {"ChangeTarget", "Change target ID in stripe patterns", FsckRepairAction_CHANGETARGET},
      {"RecreateFsID", "Recreate dentry-by-id file", FsckRepairAction_RECREATEFSID},
      {"RecreateDentry", "Recreate directory entry file", FsckRepairAction_RECREATEDENTRY},
      {"FixPermissions", "Fix permissions", FsckRepairAction_FIXPERMISSIONS},
      {"MoveChunk", "Move chunk", FsckRepairAction_MOVECHUNK},
      {"RepairDuplicateInode", "Repair duplicate inode", FsckRepairAction_REPAIRDUPLICATEINODE},
      {"UpdateOldStyledHardlinks", "Update metadata of old styled hardlinks to new format",
         FsckRepairAction_UPDATEOLDTYLEDHARDLINKS},
      {"Nothing", "Do nothing", FsckRepairAction_NOTHING},
      {0, 0, FsckRepairAction_UNDEFINED}
};

#endif /* FSCKDBTYPES_H_ */
