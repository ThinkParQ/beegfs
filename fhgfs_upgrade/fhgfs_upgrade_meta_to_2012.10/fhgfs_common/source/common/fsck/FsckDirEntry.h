#ifndef FSCKDIRENTRY_H_
#define FSCKDIRENTRY_H_

#include <common/Common.h>

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

   public:
      size_t serialize(char* outBuf);
      bool deserialize(const char* buf, size_t bufLen, unsigned* outLen);
      unsigned serialLen(void);

   private:
      std::string id; // a filesystem-wide identifier for this entry
      std::string name; // the user-friendly name
      std::string parentDirID;
      uint16_t entryOwnerNodeID;
      uint16_t inodeOwnerNodeID; // 0 for unknown owner
      /* Note : The DirEntryTypes are stored as integers in the DB;
       always keep them in sync with enum FsckDirEntryType! */
      FsckDirEntryType entryType;
      uint16_t saveNodeID;
      int saveDevice; // the device on which this dentry file is saved (according to stat)
      uint64_t saveInode; // the inode of this dentry file (according to stat)

   public:
      FsckDirEntry(std::string id, std::string name, std::string parentDirID,
         uint16_t entryOwnerNodeID, uint16_t inodeOwnerNodeID, FsckDirEntryType entryType,
         uint16_t saveNodeID, int saveDevice, uint64_t saveInode) :
         id(id), name(name), parentDirID(parentDirID), entryOwnerNodeID(entryOwnerNodeID),
         inodeOwnerNodeID(inodeOwnerNodeID), entryType(entryType), saveNodeID(saveNodeID),
         saveDevice(saveDevice), saveInode(saveInode) { };

      //only for deserialization
      FsckDirEntry() {}

      std::string getID() const
      {
         return this->id;
      }

      std::string getName() const
      {
         return this->name;
      }

      void setName(std::string name)
      {
          this->name = name;
      }

      std::string getParentDirID() const
      {
         return this->parentDirID;
      }

      void setParentDirID(std::string parentDirID)
      {
          this->parentDirID = parentDirID;
      }

      uint16_t getEntryOwnerNodeID() const
      {
         return this->entryOwnerNodeID;
      }

      void setEntryOwnerNodeID(uint16_t entryOwnerNodeID)
      {
          this->entryOwnerNodeID = entryOwnerNodeID;
      }

      uint16_t getInodeOwnerNodeID() const
      {
         return this->inodeOwnerNodeID;
      }

      void setInodeOwnerNodeID(uint16_t inodeOwnerNodeID)
      {
          this->inodeOwnerNodeID = inodeOwnerNodeID;
      }

      FsckDirEntryType getEntryType() const
      {
         return this->entryType;
      }

      void setEntryType(FsckDirEntryType entryType)
      {
          this->entryType = entryType;
      }

      uint16_t getSaveNodeID() const
      {
         return saveNodeID;
      }

      void setSaveNodeID(uint16_t saveNodeID)
      {
          this->saveNodeID = saveNodeID;
      }

      int getSaveDevice()
      {
         return this->saveDevice;
      }

      void setSaveDevice(int device)
      {
         this->saveDevice = device;
      }

      uint64_t getSaveInode()
      {
         return this->saveInode;
      }

      void setSaveInode(uint64_t inode)
      {
         this->saveInode = inode;
      }


      bool operator< (const FsckDirEntry& other)
      {
         if (id < other.id)
            return true;
         else
            return false;
      }

      bool operator== (const FsckDirEntry& other)
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
         if (saveNodeID != other.saveNodeID)
            return false;
         else
         if (saveDevice != other.saveDevice)
            return false;
         else
         if (saveInode != other.saveInode)
            return false;
         else
            return true;
      }

      bool operator!= (const FsckDirEntry& other)
      {
         return !(operator==( other ) );
      }

      void update(std::string id, std::string name, std::string parentDirID,
         uint16_t entryOwnerNodeID, uint16_t inodeOwnerNodeID, FsckDirEntryType entryType,
         uint16_t saveNodeID, int saveDevice, uint64_t saveInode)
       {
          this->id = id;
          this->name = name;
          this->parentDirID = parentDirID;
          this->entryOwnerNodeID = entryOwnerNodeID;
          this->inodeOwnerNodeID = inodeOwnerNodeID;
          this->entryType = entryType;
          this->saveNodeID = saveNodeID;
          this->saveDevice = saveDevice;
          this->saveInode = saveInode;
       }
};

#endif /* FSCKDIRENTRY_H_ */
