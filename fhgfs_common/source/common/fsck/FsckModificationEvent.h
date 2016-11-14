#ifndef FSCKMODIFICATIONEVENT_H_
#define FSCKMODIFICATIONEVENT_H_

#include <common/Common.h>
#include <common/toolkit/MetadataTk.h>
#include <common/toolkit/serialization/Serialization.h>

class FsckModificationEvent;

typedef std::list<FsckModificationEvent> FsckModificationEventList;
typedef FsckModificationEventList::iterator FsckModificationEventListIter;

class FsckModificationEvent
{
   friend class TestDatabase;

   private:
      ModificationEventType eventType;
      std::string entryID;

   public:
      /*
       * @param eventType
       * @param entryID
       */
      FsckModificationEvent(ModificationEventType eventType, const std::string& entryID):
         eventType(eventType), entryID(entryID)
      {
      }

      // only for deserialization!
      FsckModificationEvent()
      {
      }

      // getter/setter
      ModificationEventType getEventType() const
      {
         return this->eventType;
      }

      const std::string& getEntryID() const
      {
         return this->entryID;
      }

      bool operator<(const FsckModificationEvent& other) const
      {
         if ( eventType < other.eventType )
            return true;
         else
         if ( ( eventType == other.eventType ) && ( entryID < other.entryID ))
            return true;
         else
            return false;
      }

      bool operator==(const FsckModificationEvent& other) const
      {
         if ( eventType != other.eventType )
            return false;
         else
         if ( entryID.compare(other.entryID) )
            return false;
         else
            return true;
      }

      bool operator!= (const FsckModificationEvent& other) const
      {
         return !(operator==( other ) );
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::as<uint8_t>(obj->eventType)
            % serdes::stringAlign4(obj->entryID);
      }
};

template<>
struct ListSerializationHasLength<FsckModificationEvent> : boost::false_type {};

#endif /* FSCKMODIFICATIONEVENT_H_ */
