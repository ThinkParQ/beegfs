#pragma once

#include <common/net/message/NetMessage.h>
#include <common/toolkit/serialization/Serialization.h>
#include <common/storage/EntryInfo.h>


class ChunkBalanceMsg : public MirroredMessageBase<ChunkBalanceMsg>
{
   public:
     ChunkBalanceMsg(uint16_t targetID, uint16_t destinationID, EntryInfo* entryInfo, StringList* relativePaths) :
         BaseType(NETMSGTYPE_ChunkBalance)
      {
         this->targetID = targetID;
         this->destinationID = destinationID;
         this->relativePaths = relativePaths;
         this->entryInfoPtr = entryInfo;
      }

      ChunkBalanceMsg() : BaseType(NETMSGTYPE_ChunkBalance)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->targetID
            % obj->destinationID
            % serdes::backedPtr(obj->entryInfoPtr, obj->entryInfo)
            % serdes::backedPtr(obj->relativePaths, obj->parsed.relativePaths);
      }

      bool supportsMirroring() const {return true; }
   private:
      uint16_t targetID;
      uint16_t destinationID;

      // for serialization
      StringList* relativePaths;
      EntryInfo* entryInfoPtr;

      // for deserialization
      struct {
         StringList relativePaths;
      } parsed;
      EntryInfo entryInfo;

   public:
      uint16_t getTargetID() const
      {
         return targetID;
      }

      uint16_t getDestinationID() const
      {
         return destinationID;
      }

      StringList& getRelativePaths()
      {
         return *relativePaths;
      }

      EntryInfo* getEntryInfo(void)
      {
         return &this->entryInfo;
      }

};


