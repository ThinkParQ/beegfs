#include "FsckDB.h"

#include <program/Program.h>
#include <common/toolkit/StorageTk.h>
#include <common/toolkit/StringTk.h>
#include <database/FsckDBException.h>
#include <database/FsckDBTable.h>
#include <toolkit/FsckTkEx.h>

#include <cstdio>

FsckDB::FsckDB(const std::string& databasePath, size_t fragmentSize, size_t nameCacheLimit,
      bool allowCreate)
   : log("FsckDB"),
     databasePath(databasePath),
     dentryTable(new FsckDBDentryTable(databasePath, fragmentSize, nameCacheLimit, allowCreate) ),
     fileInodesTable(new FsckDBFileInodesTable(databasePath, fragmentSize, allowCreate) ),
     dirInodesTable(new FsckDBDirInodesTable(databasePath, fragmentSize, allowCreate) ),
     chunksTable(new FsckDBChunksTable(databasePath, fragmentSize, allowCreate) ),
     contDirsTable(new FsckDBContDirsTable(databasePath, fragmentSize, allowCreate) ),
     fsIDsTable(new FsckDBFsIDsTable(databasePath, fragmentSize, allowCreate) ),
     usedTargetIDsTable(new FsckDBUsedTargetIDsTable(databasePath, fragmentSize, allowCreate) ),
     modificationEventsTable(new FsckDBModificationEventsTable(databasePath, fragmentSize,
         allowCreate) ),
     malformedChunks(databasePath + "/malformedChunks")
{
}

void FsckDB::clear()
{
   this->dentryTable->clear();
   this->fileInodesTable->clear();
   this->dirInodesTable->clear();
   this->chunksTable->clear();
   this->contDirsTable->clear();
   this->fsIDsTable->clear();
   this->usedTargetIDsTable->clear();
   this->modificationEventsTable->clear();
   this->malformedChunks.clear();
}
