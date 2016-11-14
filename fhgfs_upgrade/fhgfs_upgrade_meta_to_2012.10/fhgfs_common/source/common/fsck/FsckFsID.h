#ifndef FSCKFSIDS_H_
#define FSCKFSIDS_H_

#include <common/Common.h>

class FsckFsID;

typedef std::list<FsckFsID> FsckFsIDList;
typedef FsckFsIDList::iterator FsckFsIDListIter;

class FsckFsID
{
   friend class TestDatabase;

   public:
      size_t serialize(char* outBuf);
      bool deserialize(const char* buf, size_t bufLen, unsigned* outLen);
      unsigned serialLen();

   private:
      std::string id;
      std::string parentDirID;
      uint16_t saveNodeID;
      int saveDevice;
      uint64_t saveInode;

   public:
      /*
       * @param id the entryID
       * @param parentDirID id of the parent dir
       * @param saveNodeID the id of the node, on which the fsid file is saved
       * @param saveDevice the underlying device, on which the file is saved
       * @param saveInode the underlying inode, which holds the file
       * */
      FsckFsID(std::string id, std::string parentDirID, uint16_t saveNodeID, int saveDevice,
         uint64_t saveInode) :
         id(id), parentDirID(parentDirID), saveNodeID(saveNodeID), saveDevice(saveDevice),
         saveInode(saveInode)
      {
      }

      // only for deserialization!
      FsckFsID()
      {
      }

      std::string getID() const
      {
         return this->id;
      }

      std::string getParentDirID() const
      {
         return this->parentDirID;
      }

      uint16_t getSaveNodeID() const
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

      bool operator<(const FsckFsID& other)
      {
         if ( id < other.id )
            return true;
         else
            return false;
      }

      bool operator==(const FsckFsID& other)
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
            return true;
      }

      bool operator!= (const FsckFsID& other)
      {
         return !(operator==( other ) );
      }

      void update(std::string id, std::string parentDirID, uint16_t saveNodeID, int saveDevice,
         uint64_t saveInode)
      {
         this->id = id;
         this->parentDirID = parentDirID;
         this->saveNodeID = saveNodeID;
         this->saveDevice = saveDevice;
         this->saveInode = saveInode;
      }
};

#endif /* FSCKFSIDS_H_ */
