/*
 * class EntryInfo - required information to find an inode on a server
 */

#ifndef ENTRYINFO_H_
#define ENTRYINFO_H_

#include <common/storage/StorageDefinitions.h>
#include <common/storage/EntryOwnerInfo.h>


#define ENTRYINFO_FLAG_INLINED    1


class EntryInfo; // forward declaration


typedef std::list<EntryInfo> EntryInfoList;
typedef EntryInfoList::iterator EntryInfoListIter;
typedef EntryInfoList::const_iterator EntryInfoListConstIter;


/**
 * Information about a file/directory
 */
class EntryInfo
{
   public:
      EntryInfo()
      {
         this->ownerNodeID   = 0;
         this->entryType     = DirEntryType_INVALID;
         this->statFlags     = 0;
      };

      EntryInfo(uint16_t ownerNodeID, std::string parentEntryID, std::string entryID,
         std::string fileName, DirEntryType entryType, int statFlags) :
         parentEntryID(parentEntryID), entryID(entryID), fileName(fileName)
      {
         this->ownerNodeID   = ownerNodeID;
         //this->parentEntryID = parentEntryID; // set in initializer list
         //this->entryID       = entryID; // set in initializer list
         //this->fileName      = fileName; // set in intializer list
         this->entryType     = entryType;
         this->statFlags     = statFlags;
      }


   private:
      uint16_t ownerNodeID; // nodeID of the metadata server that has the inode
      std::string parentEntryID; // entryID of the parent dir
      std::string entryID; // entryID of the actual entry
      std::string fileName; // file/dir name of the actual entry

      DirEntryType entryType;
      int statFlags; // additional stat flags (e.g. DIRENTRY_FEATURE_INODE_INLINE)


   public:

      size_t serialize(char* outBuf);
      bool deserialize(const char* buf, size_t bufLen, unsigned* outLen);
      unsigned serialLen(void);


      // inliners

      void update(uint16_t ownerNodeID, std::string parentEntryID,
         std::string entryID, std::string fileName, DirEntryType entryType, int statFlags)
      {
         this->ownerNodeID   = ownerNodeID;
         this->parentEntryID = parentEntryID;
         this->entryID       = entryID;
         this->fileName      = fileName;
         this->entryType     = entryType;
         this->statFlags     = statFlags;
      }

      void updateFromOwnerInfo(EntryOwnerInfo* entryOwnerInfo)
      {
         this->ownerNodeID   = entryOwnerInfo->getOwnerNodeID();
         this->parentEntryID = entryOwnerInfo->getParentEntryID();
         this->entryID       = entryOwnerInfo->getEntryID();
         this->fileName      = entryOwnerInfo->getFileName();
         this->entryType     = entryOwnerInfo->getEntryType();
         this->statFlags     = entryOwnerInfo->getStatFlags();
      }

      void set(EntryInfo *newEntryInfo)
      {
         this->ownerNodeID   = newEntryInfo->getOwnerNodeID();
         this->parentEntryID = newEntryInfo->getParentEntryID();
         this->entryID       = newEntryInfo->getEntryID();
         this->fileName      = newEntryInfo->getFileName();
         this->entryType     = newEntryInfo->getEntryType();
         this->statFlags     = newEntryInfo->getStatFlags();
      }

      void setParentEntryID(std::string& newParentEntryID)
      {
         this->parentEntryID = newParentEntryID;
      }

      /**
       * Set or unset the ENTRYINFO_FLAG_INLINED flag.
       */
      void setInodeInlinedFlag(bool isInlined)
      {
         if (isInlined)
            this->statFlags |= ENTRYINFO_FLAG_INLINED;
         else
            this->statFlags &= ~(ENTRYINFO_FLAG_INLINED);
      }

      /**
       * Get ownerInfo from entryInfo values
       */
      void getOwnerInfo(int depth, EntryOwnerInfo* outOwnerInfo)
      {
         outOwnerInfo->update(depth, this->ownerNodeID, this->parentEntryID, this->entryID,
            this->fileName, this->entryType, this->statFlags);
      }

      uint16_t getOwnerNodeID(void)
      {
         return this->ownerNodeID;
      }

      std::string getParentEntryID(void)
      {
         return this->parentEntryID;
      }

      std::string getEntryID(void)
      {
         return this->entryID;
      }

      std::string getFileName(void)
      {
         return this->fileName;
      }

      DirEntryType getEntryType(void)
      {
         return this->entryType;
      }

      int getStatFlags(void)
      {
         return this->statFlags;
      }
};

#endif /* ENTRYINFO_H_ */
