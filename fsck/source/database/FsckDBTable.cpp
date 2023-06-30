#include "FsckDBTable.h"

#include <database/FsckDBException.h>
#include <toolkit/FsckTkEx.h>

#include <boost/make_shared.hpp>

static db::DirEntry fsckDirEntryToDbDirEntry(const FsckDirEntry& dentry)
{
   db::DirEntry result = {
      db::EntryID::fromStr(dentry.getID() ),
      db::EntryID::fromStr(dentry.getParentDirID() ),

      dentry.getEntryOwnerNodeID().val(),
      dentry.getInodeOwnerNodeID().val(),

      dentry.getSaveNodeID().val(), dentry.getSaveDevice(), dentry.getSaveInode(),

      {},

      dentry.getInternalID(),

      dentry.getEntryType(),
      dentry.getName().size() < sizeof(result.name.inlined.text),
      dentry.getHasInlinedInode(),
      dentry.getIsBuddyMirrored(),
   };

   if(result.fileNameInlined) {
      ::memset(result.name.inlined.text, '\0', sizeof(result.name.inlined.text));
      ::strncpy(result.name.inlined.text, dentry.getName().c_str(),
            sizeof(result.name.inlined.text) - 1);
   }

   return result;
}

void FsckDBDentryTable::insert(FsckDirEntryList& dentries, const BulkHandle* handle)
{
   NameBuffer& names = handle ? *handle->nameBuffer : getNameBuffer(0);

   for(FsckDirEntryListIter it = dentries.begin(), end = dentries.end(); it != end; ++it)
   {
      db::DirEntry dentry = fsckDirEntryToDbDirEntry(*it);
      ByParent link = { dentry.parentDirID, dentry.id, dentry.entryType };

      if(!dentry.fileNameInlined)
      {
         dentry.name.extended.fileID = names.id();
         dentry.name.extended.fileOffset = names.put(it->getName() );
      }

      if(handle)
      {
         handle->dentries->append(dentry);
         handle->byParent->append(link);
      }
      else
      {
         this->table.insert(dentry);
         this->byParent.insert(link);
      }
   }
}

void FsckDBDentryTable::updateFieldsExceptParent(FsckDirEntryList& dentries)
{
   for(FsckDirEntryListIter it = dentries.begin(), end = dentries.end(); it != end; ++it)
   {
      db::DirEntry dentry = fsckDirEntryToDbDirEntry(*it);

      this->table.remove(dentry.pkey() );
      this->table.insert(dentry);
   }
}

void FsckDBDentryTable::remove(FsckDirEntryList& dentries)
{
   for(FsckDirEntryListIter it = dentries.begin(), end = dentries.end(); it != end; ++it)
      this->table.remove(fsckDirEntryToDbDirEntry(*it).pkey() );
}

Table<db::DirEntry>::QueryType FsckDBDentryTable::get()
{
   this->table.commitChanges();
   return this->table.cursor();
}

Table<FsckDBDentryTable::ByParent>::QueryType FsckDBDentryTable::getByParent()
{
   this->byParent.commitChanges();
   return this->byParent.cursor();
}

std::pair<bool, db::DirEntry> FsckDBDentryTable::getAnyFor(db::EntryID id)
{
   this->table.commitChanges();

   struct ops
   {
      static db::EntryID onlyID(const db::DirEntry::KeyType& key) { return boost::get<0>(key); }
   };

   return this->table.getByKeyProjection(id, ops::onlyID);
}

std::string FsckDBDentryTable::getNameOf(const db::DirEntry& dentry)
{
   if(dentry.fileNameInlined)
      return dentry.name.inlined.text;

   return getNameBuffer(dentry.name.extended.fileID).get(dentry.name.extended.fileOffset);
}

std::string FsckDBDentryTable::getPathOf(const db::DirEntry& dentry)
{
   std::string result = getNameOf(dentry);

   db::EntryID parent = dentry.parentDirID;
   unsigned depth = 0;

   while(!parent.isSpecial() )
   {
      depth++;
      if(depth > 255)
         return "[<unresolved>]/" + result;

      const NameCacheEntry* cacheEntry = getFromCache(parent);
      if(cacheEntry)
      {
         result = cacheEntry->name + "/" + result;
         parent = cacheEntry->parent;
         continue;
      }

      std::pair<bool, db::DirEntry> item = getAnyFor(parent);
      if(!item.first)
         return "[<unresolved>]/" + result;

      std::string parentName = getNameOf(item.second);

      result = parentName + "/" + result;
      parent = item.second.parentDirID;

      addToCache(item.second, parentName);
   }

   return getNameOf(parent) + "/" + result;
}



void FsckDBFileInodesTable::insert(FsckFileInodeList& fileInodes, const BulkHandle* handle)
{
   for(FsckFileInodeListIter it = fileInodes.begin(), end = fileInodes.end(); it != end; ++it)
   {
      const std::vector<uint16_t>& stripes = it->getStripeTargets();

      db::FileInode inode = {
         db::EntryID::fromStr(it->getID() ),
         db::EntryID::fromStr(it->getParentDirID() ),
         db::EntryID::fromStr(it->getPathInfo()->getOrigParentEntryID() ),

         it->getParentNodeID().val(),
         it->getSaveNodeID().val(),
         it->getPathInfo()->getOrigUID(),

         it->getUserID(), it->getGroupID(),

         (uint64_t) it->getFileSize(), it->getUsedBlocks(),

         it->getNumHardLinks(),

         it->getSaveInode(),
         it->getSaveDevice(),

         it->getChunkSize(),

         {},

         it->getIsInlined(),
         uint32_t(it->getPathInfo()->getFlags() ),
         it->getStripePatternType(),
         uint32_t(stripes.size() ),
         it->getReadable(),
         it->getIsBuddyMirrored(),
         it->getIsMismirrored(),
      };

      db::StripeTargets extraTargets = { inode.id, {}, 0 };

      for(size_t i = 0; i < stripes.size(); i++)
      {
         if(i < inode.NTARGETS)
            inode.targets[i] = stripes[i];
         else
         {
            size_t offsetInPattern = (i - inode.NTARGETS) % extraTargets.NTARGETS;

            if(offsetInPattern == 0)
               extraTargets.firstTargetIndex = i;

            extraTargets.targets[offsetInPattern] = stripes[i];

            if(offsetInPattern == extraTargets.NTARGETS - 1)
            {
               if(handle)
                  std::get<1>(*handle)->append(extraTargets);
               else
                  this->targets.insert(extraTargets);

               extraTargets.firstTargetIndex = 0;
            }
         }
      }

      if(handle)
      {
         std::get<0>(*handle)->append(inode);
         if(extraTargets.firstTargetIndex != 0)
            std::get<1>(*handle)->append(extraTargets);
      }
      else
      {
         this->inodes.insert(inode);
         if(extraTargets.firstTargetIndex != 0)
            this->targets.insert(extraTargets);
      }
   }
}

void FsckDBFileInodesTable::update(FsckFileInodeList& inodes)
{
   for(FsckFileInodeListIter it = inodes.begin(), end = inodes.end(); it != end; ++it)
   {
      FsckFileInodeList list(1, *it);

      remove(list);
      insert(list);
   }
}

void FsckDBFileInodesTable::remove(FsckFileInodeList& fileInodes)
{
   for(FsckFileInodeListIter it = fileInodes.begin(), end = fileInodes.end(); it != end; ++it)
   {
      this->inodes.remove(db::EntryID::fromStr(it->getID() ) );
      this->targets.remove(db::EntryID::fromStr(it->getID() ) );
   }
}

FsckDBFileInodesTable::InodesQueryType FsckDBFileInodesTable::getInodes()
{
   this->inodes.commitChanges();
   return
      this->inodes.cursor()
      | db::groupBy(UniqueInlinedInode() )
      | db::select(SelectFirst() );
}

Table<db::StripeTargets, true>::QueryType FsckDBFileInodesTable::getTargets()
{
   this->targets.commitChanges();
   return this->targets.cursor();
}

UInt16Vector FsckDBFileInodesTable::getStripeTargetsByKey(const db::EntryID& id)
{
   UInt16Vector allStripeTargets;
   std::pair<bool, db::FileInode> result = this->get(id.str());

   if (!result.first)
      return allStripeTargets;

   // firstly copy stripeTargets from FileInode object
   auto inode = result.second;
   uint32_t targetArrSize = inode.NTARGETS;
   uint32_t numTargets = (inode.stripePatternSize > targetArrSize) ? targetArrSize
      : inode.stripePatternSize;

   std::copy(inode.targets, inode.targets + numTargets, std::back_inserter(allStripeTargets));

   // if extraTargets are present then get them from targets table
   uint32_t numExtraTargets = inode.stripePatternSize - numTargets;
   if (numExtraTargets)
   {
      this->targets.commitChanges();
      auto dbRes = this->targets.getAllByKey(id);

      while (dbRes.step())
      {
         auto elem = dbRes.get();

         for (int i=0; i<elem->NTARGETS; i++)
         {
            allStripeTargets.push_back(elem->targets[i]);
            numExtraTargets -= 1;
            if (numExtraTargets == 0) break;
         }

         if (numExtraTargets == 0) break;
      }
   }

   return allStripeTargets;
}

std::pair<bool, db::FileInode> FsckDBFileInodesTable::get(std::string id)
{
   this->inodes.commitChanges();
   return this->inodes.getByKey(db::EntryID::fromStr(id) );
}



void FsckDBDirInodesTable::insert(FsckDirInodeList& dirInodes, const BulkHandle* handle)
{
   for(FsckDirInodeListIter it = dirInodes.begin(), end = dirInodes.end(); it != end; ++it)
   {
      db::DirInode inode = {
         db::EntryID::fromStr(it->getID() ),
         db::EntryID::fromStr(it->getParentDirID() ),

         it->getParentNodeID().val(),
         it->getOwnerNodeID().val(),
         it->getSaveNodeID().val(),

         it->getStripePatternType(),
         it->getReadable(),
         it->getIsBuddyMirrored(),
         it->getIsMismirrored(),

         (uint64_t) it->getSize(),
         it->getNumHardLinks(),
      };

      if(handle)
         (*handle)->append(inode);
      else
         this->table.insert(inode);
   }
}

void FsckDBDirInodesTable::update(FsckDirInodeList& inodes)
{
   for(FsckDirInodeListIter it = inodes.begin(), end = inodes.end(); it != end; ++it)
   {
      FsckDirInodeList list(1, *it);

      remove(list);
      insert(list);
   }
}

void FsckDBDirInodesTable::remove(FsckDirInodeList& dirInodes)
{
   for(FsckDirInodeListIter it = dirInodes.begin(), end = dirInodes.end(); it != end; ++it)
      this->table.remove(db::EntryID::fromStr(it->getID() ) );
}

Table<db::DirInode>::QueryType FsckDBDirInodesTable::get()
{
   this->table.commitChanges();
   return this->table.cursor();
}

std::pair<bool, FsckDirInode> FsckDBDirInodesTable::get(std::string id)
{
   this->table.commitChanges();
   return this->table.getByKey(db::EntryID::fromStr(id) );
}



static db::Chunk fsckChunkToDbChunk(FsckChunk& chunk)
{
   db::Chunk result = {
      db::EntryID::fromStr(chunk.getID() ),
      chunk.getTargetID(), chunk.getBuddyGroupID(),
      {},
      chunk.getFileSize(), chunk.getUsedBlocks(),
      chunk.getUserID(), chunk.getGroupID(),
   };

   strncpy(result.savedPath, chunk.getSavedPath()->str().c_str(),
      sizeof(result.savedPath) - 1);

   return result;
}

void FsckDBChunksTable::insert(FsckChunkList& chunks, const BulkHandle* handle)
{
   for(FsckChunkListIter it = chunks.begin(), end = chunks.end(); it != end; ++it)
   {
      if(handle)
         (*handle)->append(fsckChunkToDbChunk(*it) );
      else
         this->table.insert(fsckChunkToDbChunk(*it) );
   }
}

void FsckDBChunksTable::update(FsckChunkList& chunks)
{
   for(FsckChunkListIter it = chunks.begin(), end = chunks.end(); it != end; ++it)
   {
      db::Chunk chunk = fsckChunkToDbChunk(*it);

      this->table.remove(chunk.pkey() );
      this->table.insert(chunk);
   }
}

void FsckDBChunksTable::remove(const db::Chunk::KeyType& id)
{
   this->table.remove(id);
}

Table<db::Chunk>::QueryType FsckDBChunksTable::get()
{
   this->table.commitChanges();
   return this->table.cursor();
}



static db::ContDir fsckContDirToDbContDir(FsckContDir& contDir)
{
   return {
      db::EntryID::fromStr(contDir.getID()),
      contDir.getSaveNodeID().val(),
      contDir.getIsBuddyMirrored(),
   };
}

void FsckDBContDirsTable::insert(FsckContDirList& contDirs, const BulkHandle* handle)
{
   for(FsckContDirListIter it = contDirs.begin(), end = contDirs.end(); it != end; ++it)
   {
      if(handle)
         (*handle)->append(fsckContDirToDbContDir(*it) );
      else
         this->table.insert(fsckContDirToDbContDir(*it) );
   }
}

Table<db::ContDir>::QueryType FsckDBContDirsTable::get()
{
   this->table.commitChanges();
   return this->table.cursor();
}



static db::FsID fsckFsIDToDbFsID(FsckFsID& id)
{
   return {
      db::EntryID::fromStr(id.getID() ),
      db::EntryID::fromStr(id.getParentDirID() ),
      id.getSaveNodeID().val(), id.getSaveDevice(), id.getSaveInode(),
      id.getIsBuddyMirrored(),
   };
}

void FsckDBFsIDsTable::insert(FsckFsIDList& fsIDs, const BulkHandle* handle)
{
   for(FsckFsIDListIter it = fsIDs.begin(), end = fsIDs.end(); it != end; ++it)
   {
      if(handle)
         (*handle)->append(fsckFsIDToDbFsID(*it) );
      else
         this->table.insert(fsckFsIDToDbFsID(*it) );
   }
}

void FsckDBFsIDsTable::remove(FsckFsIDList& fsIDs)
{
   for(FsckFsIDListIter it = fsIDs.begin(), end = fsIDs.end(); it != end; ++it)
      this->table.remove(fsckFsIDToDbFsID(*it).pkey() );
}

Table<db::FsID>::QueryType FsckDBFsIDsTable::get()
{
   this->table.commitChanges();
   return this->table.cursor();
}



void FsckDBUsedTargetIDsTable::insert(FsckTargetIDList& targetIDs, const BulkHandle& handle)
{
   for(FsckTargetIDListIter it = targetIDs.begin(), end = targetIDs.end(); it != end; ++it)
   {
      db::UsedTarget target = { it->getID(), it->getTargetIDType() };
      handle->append(target);
   }
}



void FsckDBModificationEventsTable::insert(const FsckModificationEventList& events,
   const BulkHandle& handle)
{
   for(std::list<FsckModificationEvent>::const_iterator it = events.begin(), end = events.end();
         it != end; ++it)
   {
      db::ModificationEvent event = { db::EntryID::fromStr(it->getEntryID() ) };
      handle->append(event);
   }
}
