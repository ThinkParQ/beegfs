#ifndef FSCKDUPLICATEINODEINFO_H_
#define FSCKDUPLICATEINODEINFO_H_

#include <common/Common.h>
#include <common/toolkit/serialization/Serialization.h>

class FsckDuplicateInodeInfo;

typedef std::vector<FsckDuplicateInodeInfo> FsckDuplicateInodeInfoVector;
typedef FsckDuplicateInodeInfoVector::iterator FsckDuplicateInodeInfoListIter;

/**
 *
 * class to store inode specific information for duplicate file and directory inode(s)
 */
class FsckDuplicateInodeInfo
{
   public:
      FsckDuplicateInodeInfo() = default;

      FsckDuplicateInodeInfo(
         const std::string& id,
         const std::string& parentDirId,
         uint32_t nodeId,
         bool inlined,
         bool mirrored,
         DirEntryType entryType) :
         entryID(id), parentDirID(parentDirId), saveNodeID(nodeId), isInlined(inlined),
         isBuddyMirrored(mirrored), dirEntryType(entryType)
   {
   }

   private:
      std::string entryID;
      std::string parentDirID;
      uint32_t saveNodeID;
      bool isInlined;
      bool isBuddyMirrored;
      DirEntryType dirEntryType;

   public:
      const std::string& getID() const { return entryID; }

      const std::string& getParentDirID() const { return parentDirID; }

      uint32_t getSaveNodeID() const { return saveNodeID; }
      void setSaveNodeID(uint32_t val) { saveNodeID = val; }

      bool getIsInlined() const { return isInlined; }

      bool getIsBuddyMirrored() const { return isBuddyMirrored; }

      DirEntryType getDirEntryType() const { return dirEntryType; }

      bool operator<(const FsckDuplicateInodeInfo& other) const
      {
         return parentDirID < other.parentDirID ||
               saveNodeID < other.saveNodeID ||
               isInlined < other.isInlined ||
               isBuddyMirrored < other.isBuddyMirrored ||
               dirEntryType < other.dirEntryType;
      }

      bool operator==(const FsckDuplicateInodeInfo& other) const
      {
         return parentDirID == other.parentDirID &&
            saveNodeID == other.saveNodeID &&
            isInlined == other.isInlined &&
            isBuddyMirrored == other.isBuddyMirrored &&
            dirEntryType == other.dirEntryType;
      }

      bool operator!=(const FsckDuplicateInodeInfo& other) const
      {
         return !(operator==( other ));
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::stringAlign4(obj->entryID)
            % serdes::stringAlign4(obj->parentDirID)
            % obj->saveNodeID
            % obj->isBuddyMirrored
            % obj->dirEntryType;
      }
};

template<>
struct ListSerializationHasLength<FsckDuplicateInodeInfo> : boost::false_type {};

#endif /* FSCKDUPLICATEINODEINFO_H_ */
