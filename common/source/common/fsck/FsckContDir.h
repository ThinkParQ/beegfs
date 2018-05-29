#ifndef FSCKCONTDIR_H_
#define FSCKCONTDIR_H_

#include <common/Common.h>
#include <common/nodes/NumNodeID.h>
#include <common/toolkit/serialization/Serialization.h>

class FsckContDir;

typedef std::list<FsckContDir> FsckContDirList;
typedef FsckContDirList::iterator FsckContDirListIter;

class FsckContDir
{
   private:
      std::string id;
      NumNodeID saveNodeID;
      bool isBuddyMirrored;

   public:
      FsckContDir(const std::string& id, NumNodeID saveNodeID, bool isBuddyMirrored) :
         id(id), saveNodeID(saveNodeID), isBuddyMirrored(isBuddyMirrored)
      {
      }

      //only for deserialization
      FsckContDir() {}

      const std::string& getID() const
      {
         return this->id;
      }

      NumNodeID getSaveNodeID() const
      {
         return saveNodeID;
      }

      bool getIsBuddyMirrored() const { return isBuddyMirrored; }

      bool operator< (const FsckContDir& other) const
      {
         if (id < other.id)
            return true;
         else
            return false;
      }

      bool operator== (const FsckContDir& other) const
      {
         return id == other.id &&
            saveNodeID == other.saveNodeID &&
            isBuddyMirrored == other.isBuddyMirrored;
      }

      bool operator!= (const FsckContDir& other) const
      {
         return !(operator==( other ) );
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->id
            % obj->saveNodeID
            % obj->isBuddyMirrored;
      }
};

template<>
struct ListSerializationHasLength<FsckContDir> : boost::false_type {};

#endif /* FSCKCONTDIR_H_ */
