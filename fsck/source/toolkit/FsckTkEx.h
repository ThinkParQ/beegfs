#ifndef FSCKTKEX_H_
#define FSCKTKEX_H_

#include <common/app/log/LogContext.h>
#include <common/fsck/FsckDirEntry.h>
#include <common/fsck/FsckChunk.h>
#include <common/fsck/FsckContDir.h>
#include <common/fsck/FsckDirEntry.h>
#include <common/fsck/FsckDirInode.h>
#include <common/fsck/FsckFileInode.h>
#include <common/fsck/FsckFsID.h>
#include <common/net/message/nodes/HeartbeatRequestMsg.h>
#include <common/net/message/nodes/HeartbeatMsg.h>
#include <common/nodes/NodeStore.h>
#include <common/threading/PThread.h>
#include <common/toolkit/NodesTk.h>
#include <common/toolkit/MessagingTk.h>
#include <toolkit/FsckDefinitions.h>

/* OutputOption Flags for fsckOutput */
#define OutputOptions_NONE                0
#define OutputOptions_LINEBREAK           1
#define OutputOptions_DOUBLELINEBREAK     1 << 1
#define OutputOptions_HEADLINE            1 << 2
#define OutputOptions_FLUSH               1 << 3
#define OutputOptions_ADDLINEBREAKBEFORE  1 << 4
#define OutputOptions_COLORGREEN          1 << 5
#define OutputOptions_COLORRED            1 << 6
#define OutputOptions_LINEDELETE          1 << 7
#define OutputOptions_NOSTDOUT            1 << 8
#define OutputOptions_NOLOG               1 << 9
#define OutputOptions_STDERR              1 << 10

#define OutputColor_NORMAL "\033[0m";
#define OutputColor_GREEN "\033[32m";
#define OutputColor_RED "\033[31m";

#define SAFE_FPRINTF(stream, fmt, args...) \
   do{ if(stream) {fprintf(stream, fmt, ##args);} } while(0)

/*
 * calculating with:
 * DirEntry    76+256+28 Byte (space for dentry + longest name + one index)
 * FileInode   96 Byte    |
 * DirInode    56 Byte    # only the larger of these two counts, even though files are inlined
 * ContDir     16 Byte
 * FsID        40 Byte
 * chunk       88 Byte
 *
 */
#define NEEDED_DISKSPACE_META_INODE 512
#define NEEDED_DISKSPACE_STORAGE_INODE 88

class FsckTkEx
{
   public:
      // check the reachability of all nodes
      static bool checkReachability();
      // check the reachability of a given node by sending a heartbeat message
      static bool checkReachability(Node& node, NodeType nodetype);

      /*
       * a formatted output for fsck
       *
       * @param text The text to be printed
       * @param optionFlags OutputOptions_... flags (mainly for formatiing)
       */
      static void fsckOutput(std::string text, int optionFlags);
      // just print a formatted header with the version to the console
      static void printVersionHeader(bool toStdErr = false, bool noLogFile = false);
      // print the progress meter which goes round and round (-\|/-)
      static void progressMeter();

      static int64_t calcNeededSpace();
      static bool checkDiskSpace(Path& dbPath);

      static std::string getRepairActionDesc(FsckRepairAction repairAction, bool shortDesc = false);

      static FhgfsOpsErr startModificationLogging(NodeStore* metaNodes, Node& localNode,
            bool forceRestart);
      static bool stopModificationLogging(NodeStore* metaNodes);

      static bool checkConsistencyStates();

   private:
      FsckTkEx() {}

      // saves the last char output by the progress meter
      static char progressChar;
      // a mutex that is locked by output functions to make sure the output does not get messed up
      // by two threads doing output at the same time
      static Mutex outputMutex;


   public:
      // inliners
      static void fsckOutput(std::string text)
      {
         fsckOutput(text, OutputOptions_LINEBREAK);
      }
};

#endif /* FSCKTKEX_H_ */
