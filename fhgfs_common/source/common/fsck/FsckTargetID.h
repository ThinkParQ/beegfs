#ifndef FSCKTARGETID_H_
#define FSCKTARGETID_H_

#include <common/Common.h>
#include <common/toolkit/serialization/Serialization.h>

enum FsckTargetIDType
{
   FsckTargetIDType_TARGET = 0,
   FsckTargetIDType_BUDDYGROUP = 1
};

class FsckTargetID
{
   private:
      uint16_t id;
      FsckTargetIDType targetIDType;

   public:
      FsckTargetID(uint16_t id, FsckTargetIDType targetIDType) : id(id), targetIDType(targetIDType)
      {
         // all initialization done in initialization list
      };

      //only for deserialization
      FsckTargetID() {}

      uint16_t getID() const
      {
         return id;
      }

      FsckTargetIDType getTargetIDType() const
      {
         return targetIDType;
      }

      bool operator< (const FsckTargetID& other) const
      {
         if (id < other.id)
            return true;

         if (id == other.id && targetIDType < other.targetIDType)
            return true;

         return false;
      }

      bool operator== (const FsckTargetID& other) const
      {
         if (id != other.id)
            return false;
         else
         if (targetIDType != other.targetIDType)
            return false;
         else
            return true;
      }

      bool operator!= (const FsckTargetID& other) const
      {
         return !(operator==( other ) );
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->id
            % serdes::as<int32_t>(obj->targetIDType);
      }
};

typedef std::list<FsckTargetID> FsckTargetIDList;
typedef FsckTargetIDList::iterator FsckTargetIDListIter;

template<>
struct ListSerializationHasLength<FsckTargetID> : boost::false_type {};

#endif /* FSCKTARGETID_H_ */
