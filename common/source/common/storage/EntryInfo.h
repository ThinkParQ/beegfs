/*
 * class EntryInfo - required information to find an inode or chunk files
 *
 * NOTE: If you change this file, do not forget to adjust the client side EntryInfo.h
 */

#ifndef ENTRYINFO_H_
#define ENTRYINFO_H_

#include <common/nodes/NumNodeID.h>
#include <common/storage/StorageDefinitions.h>

#define ENTRYINFO_FEATURE_INLINED       1 // indicate inlined inode, might be outdated
#define ENTRYINFO_FEATURE_BUDDYMIRRORED 2 // indicates that the file/dir metadata is buddy mirrored

#define EntryInfo_PARENT_ID_UNKNOWN    "unknown"


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
       : ownerNodeID(0), entryType(DirEntryType_INVALID), featureFlags(0)
      { }

      EntryInfo(const NumNodeID ownerNodeID, const std::string& parentEntryID,
         const std::string& entryID, const std::string& fileName, const DirEntryType entryType,
         const int featureFlags) :
            ownerNodeID(ownerNodeID),
            parentEntryID(parentEntryID),
            entryID(entryID),
            fileName(fileName),
            entryType(entryType),
            featureFlags(featureFlags)
      { }

      virtual ~EntryInfo()
      {
      }

      bool operator==(const EntryInfo& other) const;

      bool operator!=(const EntryInfo& other) const { return !(*this == other); }

   protected:
      NumNodeID ownerNodeID; // nodeID of the metadata server that has the inode
      std::string parentEntryID; // entryID of the parent dir
      std::string entryID; // entryID of the actual entry
      std::string fileName; // file/dir name of the actual entry

      DirEntryType entryType;
      int32_t featureFlags; // feature flags (e.g. ENTRYINFO_FEATURE_INLINED)


   public:
      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         uint16_t padding = 0; // PADDING

         ctx
            % serdes::as<uint32_t>(obj->entryType)
            % obj->featureFlags
            % serdes::stringAlign4(obj->parentEntryID)
            % serdes::stringAlign4(obj->entryID)
            % serdes::stringAlign4(obj->fileName)
            % obj->ownerNodeID
            % padding;
      }


      // inliners

      void set(const NumNodeID ownerNodeID, const std::string& parentEntryID,
         const std::string& entryID, const std::string& fileName,
         const DirEntryType entryType, int featureFlags)
      {
         this->ownerNodeID   = ownerNodeID;
         this->parentEntryID = parentEntryID;
         this->entryID       = entryID;
         this->fileName      = fileName;
         this->entryType     = entryType;
         this->featureFlags  = featureFlags;
      }

      void set(const EntryInfo* newEntryInfo)
      {
         this->ownerNodeID   = newEntryInfo->getOwnerNodeID();
         this->parentEntryID = newEntryInfo->getParentEntryID();
         this->entryID       = newEntryInfo->getEntryID();
         this->fileName      = newEntryInfo->getFileName();
         this->entryType     = newEntryInfo->getEntryType();
         this->featureFlags  = newEntryInfo->getFeatureFlags();
      }

      void setParentEntryID(const std::string& newParentEntryID)
      {
         this->parentEntryID = newParentEntryID;
      }

      /**
       * Set or unset the ENTRYINFO_FEATURE_INLINED flag.
       */
      void setInodeInlinedFlag(const bool isInlined)
      {
         if (isInlined)
            this->featureFlags |= ENTRYINFO_FEATURE_INLINED;
         else
            this->featureFlags &= ~(ENTRYINFO_FEATURE_INLINED);
      }

      void setBuddyMirroredFlag(const bool isBuddyMirrored)
      {
         if (isBuddyMirrored)
            this->featureFlags |= ENTRYINFO_FEATURE_BUDDYMIRRORED;
         else
            this->featureFlags &= ~(ENTRYINFO_FEATURE_BUDDYMIRRORED);
      }

      NumNodeID getOwnerNodeID() const
      {
         return this->ownerNodeID;
      }

      const std::string& getParentEntryID() const
      {
         return this->parentEntryID;
      }

      const std::string& getEntryID() const
      {
         return this->entryID;
      }

      const std::string& getFileName() const
      {
         return this->fileName;
      }

      DirEntryType getEntryType() const
      {
         return this->entryType;
      }

      int getFeatureFlags() const
      {
         return this->featureFlags;
      }

      bool getIsInlined() const
      {
         return (this->featureFlags & ENTRYINFO_FEATURE_INLINED) ? true : false;
      }

      bool getIsBuddyMirrored() const
      {
         return (this->featureFlags & ENTRYINFO_FEATURE_BUDDYMIRRORED) ? true : false;
      }
};

#endif /* ENTRYINFO_H_ */
