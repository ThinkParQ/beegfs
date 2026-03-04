#pragma once

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>
#include <common/toolkit/serialization/Serialization.h>
#include <common/storage/FileEvent.h>


#define CPCHUNKPATHSMSG_FLAG_BUDDYMIRROR          1 /* given targetID is a buddymirrorgroup ID */
#define CPCHUNKPATHSMSG_FLAG_HAS_EVENT            2  /* contains file event logging information */

class CpChunkPathsMsg : public NetMessageSerdes<CpChunkPathsMsg>
{
   public:
      CpChunkPathsMsg(uint16_t targetID, uint16_t destinationID, EntryInfo* entryInfo,  std::string* relativePath, FileEvent* fileEvent) :
         BaseType(NETMSGTYPE_CpChunkPaths)
      {
         this->targetID = targetID;
         this->destinationID = destinationID;
         this->relativePath = relativePath;
         this->entryInfoPtr = entryInfo;
         this->fileEvent = *fileEvent;
      }

      CpChunkPathsMsg() : BaseType(NETMSGTYPE_CpChunkPaths)
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
            if (obj->isMsgHeaderFeatureFlagSet(CPCHUNKPATHSMSG_FLAG_HAS_EVENT))
            ctx % obj->fileEvent;
      }

      unsigned getSupportedHeaderFeatureFlagsMask() const
      {
         return CPCHUNKPATHSMSG_FLAG_BUDDYMIRROR |  CPCHUNKPATHSMSG_FLAG_HAS_EVENT;
      }

   private:
      uint16_t targetID;
      uint16_t destinationID;
      FileEvent fileEvent;

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
      
      FileEvent* getFileEvent() 
      {
         if (isMsgHeaderFeatureFlagSet(CPCHUNKPATHSMSG_FLAG_HAS_EVENT))
            return &fileEvent;
         else
            return nullptr;
      }

};

