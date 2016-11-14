#ifndef FSCKFSIDS_H_
#define FSCKFSIDS_H_

#include <common/Common.h>
#include <common/nodes/NumNodeID.h>
#include <common/toolkit/serialization/Serialization.h>

class FsckFsID;

typedef std::list<FsckFsID> FsckFsIDList;
typedef FsckFsIDList::iterator FsckFsIDListIter;

class FsckFsID
{
   friend class TestDatabase;

   private:
      std::string id;
      std::string parentDirID;
      NumNodeID saveNodeID;
      int32_t saveDevice;
      uint64_t saveInode;
      bool isBuddyMirrored;

   public:
      /*
       * @param id the entryID
       * @param parentDirID id of the parent dir
       * @param saveNodeID the id of the node, on which the fsid file is saved
       * @param saveDevice the underlying device, on which the file is saved
       * @param saveInode the underlying inode, which holds the file
       * */
      FsckFsID(const std::string& id, const std::string& parentDirID, NumNodeID saveNodeID,
         int saveDevice, uint64_t saveInode, bool isBuddyMirrored) :
         id(id), parentDirID(parentDirID), saveNodeID(saveNodeID), saveDevice(saveDevice),
         saveInode(saveInode), isBuddyMirrored(isBuddyMirrored)
      {
      }

      // only for deserialization!
      FsckFsID()
      {
      }

      const std::string& getID() const
      {
         return this->id;
      }

      const std::string& getParentDirID() const
      {
         return this->parentDirID;
      }

      NumNodeID getSaveNodeID() const
      {
         return this->saveNodeID;
      }

      int getSaveDevice() const
      {
         return this->saveDevice;
      }

      uint64_t getSaveInode() const
      {
         return this->saveInode;
      }

      bool getIsBuddyMirrored() const { return isBuddyMirrored; }

      bool operator<(const FsckFsID& other) const
      {
         if ( id < other.id )
            return true;
         else
            return false;
      }

      bool operator==(const FsckFsID& other) const
      {
         if ( id.compare(other.id) )
            return false;
         else
         if ( parentDirID.compare(other.parentDirID) )
            return false;
         else
         if ( saveNodeID != other.saveNodeID )
            return false;
         else
         if ( saveDevice != other.saveDevice )
            return false;
         else
         if ( saveInode != other.saveInode )
            return false;
         else
         if (isBuddyMirrored != other.isBuddyMirrored)
            return false;
         else
            return true;
      }

      bool operator!= (const FsckFsID& other) const
      {
         return !(operator==( other ) );
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::stringAlign4(obj->id)
            % serdes::stringAlign4(obj->parentDirID)
            % obj->saveNodeID
            % obj->saveDevice
            % obj->saveInode
            % obj->isBuddyMirrored;
      }
};

template<>
struct ListSerializationHasLength<FsckFsID> : boost::false_type {};

#endif /* FSCKFSIDS_H_ */
