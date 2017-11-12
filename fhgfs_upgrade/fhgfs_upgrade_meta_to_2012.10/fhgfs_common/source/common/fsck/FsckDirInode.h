#ifndef FSCKDIRINODE_H_
#define FSCKDIRINODE_H_

#include <common/Common.h>
#include <common/toolkit/FsckTk.h>

class FsckDirInode;

typedef std::list<FsckDirInode> FsckDirInodeList;
typedef FsckDirInodeList::iterator FsckDirInodeListIter;

class FsckDirInode
{
   friend class TestDatabase;

   public:
      FsckDirInode(std::string id, std::string parentDirID, uint16_t parentNodeID,
         uint16_t ownerNodeID, int64_t size, unsigned numHardLinks, UInt16Vector& stripeTargets,
         FsckStripePatternType stripePatternType, uint16_t saveNodeID, bool readable = true) :
         id(id), parentDirID(parentDirID), parentNodeID(parentNodeID), ownerNodeID(ownerNodeID),
         size(size), numHardLinks(numHardLinks), stripeTargets(stripeTargets),
         stripePatternType(stripePatternType), saveNodeID(saveNodeID), readable(readable)
      {
      }

      // only for deserialization
      FsckDirInode()
      {
      }

      size_t serialize(char* outBuf);
      bool deserialize(const char* buf, size_t bufLen, unsigned* outLen);
      unsigned serialLen();

   private:
      std::string id; // filesystem-wide unique string
      std::string parentDirID;
      uint16_t parentNodeID;
      uint16_t ownerNodeID;
      int64_t size; // # of subentries
      unsigned numHardLinks;
      UInt16Vector stripeTargets;
      FsckStripePatternType stripePatternType;
      uint16_t saveNodeID;
      bool readable;

   public:
      std::string getID() const
      {
         return id;
      }

      std::string getParentDirID() const
      {
         return parentDirID;
      }

      void setParentDirID(std::string parentDirID)
      {
         this->parentDirID = parentDirID;
      }

      uint16_t getParentNodeID() const
      {
         return parentNodeID;
      }

      void setParentNodeID(uint16_t parentNodeID)
      {
         this->parentNodeID = parentNodeID;
      }

      uint16_t getOwnerNodeID() const
      {
         return ownerNodeID;
      }

      void setOwnerNodeID(uint16_t ownerNodeID)
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

      uint16_t getSaveNodeID() const
      {
         return saveNodeID;
      }

      void setSaveNodeID(uint16_t saveNodeID)
      {
          this->saveNodeID = saveNodeID;
      }

      bool operator<(const FsckDirInode& other)
      {
         if ( id < other.getID() )
            return true;
         else
            return false;
      }

      bool operator==(const FsckDirInode& other)
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
         if ( saveNodeID != other.getSaveNodeID() )
           return false;
         else
         if ( size != other.size )
           return false;
         else
         if ( numHardLinks != other.numHardLinks )
           return false;
         else
         if ( readable != other.getReadable() )
           return false;
         else
           return true;
      }

      bool operator!= (const FsckDirInode& other)
      {
         return !(operator==( other ) );
      }

      void update(std::string id, std::string parentDirID, uint16_t parentNodeID,
         uint16_t ownerNodeID, int64_t size, unsigned numHardLinks, UInt16Vector& stripeTargets,
         FsckStripePatternType stripePatternType, uint16_t saveNodeID, bool readable)
      {
         this->id = id;
         this->parentDirID = parentDirID;
         this->parentNodeID = parentNodeID;
         this->ownerNodeID = ownerNodeID;
         this->size = size;
         this->numHardLinks = numHardLinks;
         this->stripeTargets = stripeTargets;
         this->stripePatternType = stripePatternType;
         this->saveNodeID = saveNodeID;
         this->readable = readable;
      }
};

#endif /* FSCKDIRINODE_H_ */
