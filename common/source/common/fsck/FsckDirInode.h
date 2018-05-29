#ifndef FSCKDIRINODE_H_
#define FSCKDIRINODE_H_

#include <common/Common.h>
#include <common/toolkit/FsckTk.h>
#include <common/toolkit/serialization/Serialization.h>

class FsckDirInode;

typedef std::list<FsckDirInode> FsckDirInodeList;
typedef FsckDirInodeList::iterator FsckDirInodeListIter;

class FsckDirInode
{
   friend class TestDatabase;

   public:
      FsckDirInode(const std::string& id, const std::string& parentDirID, NumNodeID parentNodeID,
         NumNodeID ownerNodeID, int64_t size, unsigned numHardLinks,
         const UInt16Vector& stripeTargets, FsckStripePatternType stripePatternType,
         NumNodeID saveNodeID, bool isBuddyMirrored, bool readable, bool isMismirrored) :
         id(id), parentDirID(parentDirID), parentNodeID(parentNodeID), ownerNodeID(ownerNodeID),
         size(size), numHardLinks(numHardLinks), stripeTargets(stripeTargets),
         stripePatternType(stripePatternType), saveNodeID(saveNodeID),
         isBuddyMirrored(isBuddyMirrored), readable(readable), isMismirrored(isMismirrored)
      {
      }

      // only for deserialization
      FsckDirInode()
      {
      }

   private:
      std::string id; // filesystem-wide unique string
      std::string parentDirID;
      NumNodeID parentNodeID;
      NumNodeID ownerNodeID;
      int64_t size; // # of subentries
      uint32_t numHardLinks;
      UInt16Vector stripeTargets;
      FsckStripePatternType stripePatternType;
      NumNodeID saveNodeID;
      bool isBuddyMirrored;
      bool readable;
      bool isMismirrored;

   public:
      const std::string& getID() const
      {
         return id;
      }

      const std::string& getParentDirID() const
      {
         return parentDirID;
      }

      void setParentDirID(const std::string& parentDirID)
      {
         this->parentDirID = parentDirID;
      }

      NumNodeID getParentNodeID() const
      {
         return parentNodeID;
      }

      void setParentNodeID(NumNodeID parentNodeID)
      {
         this->parentNodeID = parentNodeID;
      }

      NumNodeID getOwnerNodeID() const
      {
         return ownerNodeID;
      }

      void setOwnerNodeID(NumNodeID ownerNodeID)
      {
         this->ownerNodeID = ownerNodeID;
      }

      void setSize(int64_t size)
      {
         this->size = size;
      }

      int64_t getSize() const
      {
         return size;
      }

      void setNumHardLinks(unsigned numHardLinks)
      {
         this->numHardLinks = numHardLinks;
      }

      unsigned getNumHardLinks() const
      {
         return numHardLinks;
      }

      UInt16Vector getStripeTargets() const
      {
         return stripeTargets;
      }

      UInt16Vector* getStripeTargets()
      {
         return &stripeTargets;
      }

      void setStripeTargets(UInt16Vector& stripeTargets)
      {
         this->stripeTargets = stripeTargets;
      }

      FsckStripePatternType getStripePatternType() const
      {
         return stripePatternType;
      }

      void setStripePatternType(FsckStripePatternType stripePatternType)
      {
         this->stripePatternType = stripePatternType;
      }

      bool getReadable() const
      {
         return readable;
      }

      void setReadable(bool readable)
      {
          this->readable = readable;
      }

      NumNodeID getSaveNodeID() const
      {
         return saveNodeID;
      }

      void setSaveNodeID(NumNodeID saveNodeID)
      {
          this->saveNodeID = saveNodeID;
      }

      bool getIsBuddyMirrored() const { return isBuddyMirrored; }

      bool getIsMismirrored() const { return isMismirrored; }

      bool operator<(const FsckDirInode& other) const
      {
         if ( id < other.getID() )
            return true;
         else
            return false;
      }

      bool operator==(const FsckDirInode& other) const
      {
         if ( id.compare(other.getID()) != 0 )
            return false;
         else
         if ( parentDirID.compare(other.getParentDirID()) != 0 )
            return false;
         else
         if ( parentNodeID != other.getParentNodeID() )
            return false;
         else
         if ( ownerNodeID != other.getOwnerNodeID() )
           return false;
         else
         if ( size != other.size )
           return false;
         else
         if ( numHardLinks != other.numHardLinks )
           return false;
         else
         if ( stripeTargets != other.stripeTargets )
            return false;
         else
         if ( stripePatternType != other.stripePatternType )
            return false;
         else
         if ( saveNodeID != other.getSaveNodeID() )
           return false;
         else
         if ( readable != other.getReadable() )
           return false;
         else
         if (isBuddyMirrored != other.isBuddyMirrored)
            return false;
         else
         if (isMismirrored != other.isMismirrored)
            return false;
         else
           return true;
      }

      bool operator!= (const FsckDirInode& other) const
      {
         return !(operator==( other ) );
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::stringAlign4(obj->id)
            % serdes::stringAlign4(obj->parentDirID)
            % obj->parentNodeID
            % obj->ownerNodeID
            % obj->size
            % obj->numHardLinks
            % obj->stripeTargets
            % serdes::as<int32_t>(obj->stripePatternType)
            % obj->saveNodeID
            % obj->isBuddyMirrored
            % obj->readable
            % obj->isMismirrored;
      }
};

template<>
struct ListSerializationHasLength<FsckDirInode> : boost::false_type {};

#endif /* FSCKDIRINODE_H_ */
