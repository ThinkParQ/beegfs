/*
 * Entry information used by MetadataTk_...Owner...()
 */

#ifndef ENTRYOWNERINFO_H_
#define ENTRYOWNERINFO_H_

#include "DentryInodeMeta.h"

class EntryOwnerInfo;

/**
 * Information about an entry (file/directory/...)
 * Note: This structure is supposed to be used for MetadataTk_...Owner...() functions only, where
 *       the ownerDepth is required. Other functions better use struct/typedef EntryInfo
 */
class EntryOwnerInfo
{
   public:
      EntryOwnerInfo()
      {
         entryDepth          = 0;
         this->ownerNodeID   = 0;
         this->entryType     = DirEntryType_INVALID;
         this->statFlags     = 0;
      }

      EntryOwnerInfo(unsigned entryDepth, uint16_t ownerNodeID, std::string parentEntryID,
         std::string entryID, std::string fileName, DirEntryType entryType, int statFlags) :
         parentEntryID(parentEntryID), entryID(entryID), fileName(fileName)
      {
         this->entryDepth    = entryDepth;
         this->ownerNodeID   = ownerNodeID;
         //this->parentEntryID = parentEntryID; // set in initializer list
         //this->entryID       = entryID; // set in initializer list
         //this->fileName      = fileName; // set in initializer list
         this->entryType     = entryType;
         this->statFlags     = statFlags;
      }


   private:

      uint16_t ownerNodeID;
      std::string parentEntryID;
      std::string entryID;
      std::string fileName;
      DirEntryType entryType;
      int statFlags; // additional stat flags, such as DIRENTRY_FEATURE_INODE_INLINE

      unsigned entryDepth; // 0-based path depth (incl. root dir)
      DentryInodeMeta inodeMetaData;


   public:

      // inliners

      void update(unsigned entryDepth, uint16_t ownerNodeID, std::string parentEntryID,
         std::string entryID, std::string fileName, DirEntryType entryType, int statFlags)
      {
         this->entryDepth    = entryDepth;
         this->ownerNodeID   = ownerNodeID;
         this->parentEntryID = parentEntryID;
         this->entryID       = entryID;
         this->entryType     = entryType;
         this->statFlags     = statFlags;
      }

      void update(EntryOwnerInfo* inInfo)
      {
         this->entryDepth    = inInfo->entryDepth;
         this->ownerNodeID   = inInfo->ownerNodeID;
         this->parentEntryID = inInfo->parentEntryID;
         this->entryID       = inInfo->entryID;
         this->fileName      = inInfo->fileName;
         this->entryType     = inInfo->entryType;
         this->statFlags     = inInfo->statFlags;
      }

      uint16_t getOwnerNodeID(void)
      {
         return this->ownerNodeID;
      }

      unsigned getEntryDepth(void)
      {
         return this->entryDepth;
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


#endif /* ENTRYOWNERINFO_H_ */
