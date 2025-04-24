#pragma once

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>
#include <common/toolkit/serialization/Serialization.h>


class StripePatternUpdateMsg : public MirroredMessageBase<StripePatternUpdateMsg>
{
   public:
     StripePatternUpdateMsg(uint16_t targetID, uint16_t destinationID, EntryInfo* entryInfo,std::string* relativePath) :
         BaseType(NETMSGTYPE_StripePatternUpdate)
      {
         this->targetID = targetID;
         this->destinationID = destinationID;
         this->relativePath = relativePath;
         this->entryInfoPtr = entryInfo;
      }

      StripePatternUpdateMsg() : BaseType(NETMSGTYPE_StripePatternUpdate) 
      {
      } 
      
      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->targetID
            % obj->destinationID
            % serdes::backedPtr(obj->entryInfoPtr, obj->entryInfo)
            % serdes::backedPtr(obj->relativePath, obj->parsed.relativePath);
      }


      bool supportsMirroring() const { return true; } 
      
   private:
      uint16_t targetID;
      uint16_t destinationID;

      // for serialization
      std::string* relativePath;
      EntryInfo* entryInfoPtr;

      // for deserialization
      struct {
         std::string relativePath;
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

      std::string& getRelativePath()
      {
         return *relativePath;
      }

      EntryInfo* getEntryInfo()
      {
         return &this->entryInfo;
      }   
};


