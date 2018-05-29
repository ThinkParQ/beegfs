#ifndef FSCKDIRENTRY_H_
#define FSCKDIRENTRY_H_

#include <common/Common.h>
#include <common/nodes/NumNodeID.h>
#include <common/toolkit/serialization/Serialization.h>

#define FsckDirEntryType_ISDIR(dirEntryType)         (dirEntryType == FsckDirEntryType_DIRECTORY)
#define FsckDirEntryType_ISREGULARFILE(dirEntryType) (dirEntryType == FsckDirEntryType_REGULARFILE)
#define FsckDirEntryType_ISSYMLINK(dirEntryType) (dirEntryType == FsckDirEntryType_SYMLINK)
#define FsckDirEntryType_ISSPECIAL(dirEntryType) (dirEntryType == FsckDirEntryType_SPECIAL)
#define FsckDirEntryType_ISVALID(dirEntryType) (dirEntryType != FsckDirEntryType_INVALID)

enum FsckDirEntryType
{
   FsckDirEntryType_INVALID = 0,
   FsckDirEntryType_DIRECTORY = 1,
   FsckDirEntryType_REGULARFILE = 2,
   FsckDirEntryType_SYMLINK = 3,
   FsckDirEntryType_SPECIAL = 4 // BLOCKDEV,CHARDEV,FIFO,etc. are not important for fsck
};

class FsckDirEntry;

typedef std::list<FsckDirEntry> FsckDirEntryList;
typedef FsckDirEntryList::iterator FsckDirEntryListIter;

class FsckDirEntry
{
   friend class TestDatabase;

   private:
      std::string id; // a filesystem-wide identifier for this entry
      std::string name; // the user-friendly name
      std::string parentDirID;
      NumNodeID entryOwnerNodeID;
      NumNodeID inodeOwnerNodeID; // 0 for unknown owner
      /* Note : The DirEntryTypes are stored as integers in the DB;
       always keep them in sync with enum FsckDirEntryType! */
      FsckDirEntryType entryType;

      bool hasInlinedInode;
      bool isBuddyMirrored;

      NumNodeID saveNodeID;
      int32_t saveDevice; // the device on which this dentry file is saved (according to stat)
      uint64_t saveInode; // the inode of this dentry file (according to stat)

      // used in fsck database to identify dentries with a compact value instead of the full
      // name
      uint64_t internalID;

   public:
      FsckDirEntry(const std::string& id, const std::string& name, const std::string& parentDirID,
         NumNodeID entryOwnerNodeID, NumNodeID inodeOwnerNodeID, FsckDirEntryType entryType,
         bool hasInlinedInode, NumNodeID saveNodeID, int saveDevice, uint64_t saveInode,
         bool isBuddyMirrored, uint64_t internalID = 0)
         : id(id), name(name), parentDirID(parentDirID), entryOwnerNodeID(entryOwnerNodeID),
           inodeOwnerNodeID(inodeOwnerNodeID), entryType(entryType),
           hasInlinedInode(hasInlinedInode), isBuddyMirrored(isBuddyMirrored),
           saveNodeID(saveNodeID), saveDevice(saveDevice), saveInode(saveInode),
           internalID(internalID)
      {}

      //only for deserialization
      FsckDirEntry() {}

      const std::string& getID() const
      {
         return this->id;
      }

      void setID(const std::string& id)
      {
         this->id = id;
      }

      const std::string& getName() const
      {
         return this->name;
      }

      void setName(const std::string& name)
      {
          this->name = name;
      }

      const std::string& getParentDirID() const
      {
         return this->parentDirID;
      }

      void setParentDirID(const std::string& parentDirID)
      {
          this->parentDirID = parentDirID;
      }

      NumNodeID getEntryOwnerNodeID() const
      {
         return this->entryOwnerNodeID;
      }

      void setEntryOwnerNodeID(const NumNodeID entryOwnerNodeID)
      {
          this->entryOwnerNodeID = entryOwnerNodeID;
      }

      NumNodeID getInodeOwnerNodeID() const
      {
         return this->inodeOwnerNodeID;
      }

      void setInodeOwnerNodeID(const NumNodeID inodeOwnerNodeID)
      {
          this->inodeOwnerNodeID = inodeOwnerNodeID;
      }

      FsckDirEntryType getEntryType() const
      {
         return this->entryType;
      }

      void setEntryType(const FsckDirEntryType entryType)
      {
          this->entryType = entryType;
      }

      bool getHasInlinedInode() const
      {
         return hasInlinedInode;
      }

      void setHasInlinedInode(const bool hasInlinedInode)
      {
         this->hasInlinedInode = hasInlinedInode;
      }

      NumNodeID getSaveNodeID() const
      {
         return saveNodeID;
      }

      void setSaveNodeID(const NumNodeID saveNodeID)
      {
          this->saveNodeID = saveNodeID;
      }

      int getSaveDevice() const
      {
         return this->saveDevice;
      }

      void setSaveDevice(const int device)
      {
         this->saveDevice = device;
      }

      uint64_t getSaveInode() const
      {
         return this->saveInode;
      }

      void setSaveInode(const uint64_t inode)
      {
         this->saveInode = inode;
      }

      uint64_t getInternalID() const
      {
         return this->internalID;
      }

      bool getIsBuddyMirrored() const { return isBuddyMirrored; }


      bool operator< (const FsckDirEntry& other) const
      {
         if (id < other.id)
            return true;
         else
            return false;
      }

      bool operator== (const FsckDirEntry& other) const
      {
         if (id.compare(other.id) != 0)
            return false;
         else
         if (name.compare(other.name) != 0)
            return false;
         else
         if (parentDirID.compare(other.parentDirID) != 0)
            return false;
         else
         if (entryOwnerNodeID != other.entryOwnerNodeID)
            return false;
         else
         if (inodeOwnerNodeID != other.inodeOwnerNodeID)
            return false;
         else
         if (entryType != other.entryType)
            return false;
         else
         if (hasInlinedInode != other.hasInlinedInode)
            return false;
         else
         if (saveNodeID != other.saveNodeID)
            return false;
         else
         if (saveDevice != other.saveDevice)
            return false;
         else
         if (saveInode != other.saveInode)
            return false;
         else
         if (isBuddyMirrored != other.isBuddyMirrored)
            return false;
         else
            return true;
      }

      bool operator!= (const FsckDirEntry& other) const
      {
         return !(operator==( other ) );
      }

      void print()
      {
         std::cout
         << "id: " << id << std::endl
         << "name: " << name << std::endl
         << "parentDirID: " << parentDirID << std::endl
         << "entryOwnerNodeID: " << entryOwnerNodeID << std::endl
         << "inodeOwnerNodeID: " << inodeOwnerNodeID << std::endl
         << "entryType: " << (int)entryType << std::endl
         << "hasInlinedInode: " << (int)hasInlinedInode << std::endl
         << "saveNodeID: " << saveNodeID << std::endl
         << "saveDevice: " << (int)saveDevice << std::endl
         << "saveInode: " << saveInode << std::endl
         << "isBuddyMirrored: " << isBuddyMirrored << std::endl
         << "internalID: " << internalID << std::endl;
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::stringAlign4(obj->id)
            % serdes::stringAlign4(obj->name)
            % serdes::stringAlign4(obj->parentDirID)
            % obj->entryOwnerNodeID
            % obj->inodeOwnerNodeID
            % serdes::as<int32_t>(obj->entryType)
            % obj->hasInlinedInode
            % obj->isBuddyMirrored
            % obj->saveNodeID
            % obj->saveDevice
            % obj->saveInode;
      }
};

template<>
struct ListSerializationHasLength<FsckDirEntry> : boost::false_type {};

#endif /* FSCKDIRENTRY_H_ */
