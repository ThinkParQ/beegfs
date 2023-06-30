#ifndef MODEMOVEFILEINODE_H_
#define MODEMOVEFILEINODE_H_

#include <common/Common.h>
#include <common/net/message/storage/creating/MoveFileInodeMsg.h>
#include <common/net/message/storage/creating/MoveFileInodeRespMsg.h>
#include "Mode.h"

class ModeMoveFileInode : public Mode
{
   public:
      ModeMoveFileInode() {}
      virtual int execute();
      static void printHelp();

   private:
      std::unique_ptr<Path> path;
      FileInodeMode moveMode;
      bool useMountedPath;
      bool processCLArgs();
      bool communicate(EntryInfo* parentDirInfo, EntryInfo* fileInfo);
};

#endif /* MODEMOVEFILEINODE_H_ */
