#pragma once

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>

class SetFileDataStateMsg : public MirroredMessageBase<SetFileDataStateMsg>
{
   public:
      /**
       * @param entryInfo just a reference, so do not free it as long as you use this object!
       * @param dataState the new data state of the file
       */
      SetFileDataStateMsg(EntryInfo* entryInfo, uint8_t dataState) :
         BaseType(NETMSGTYPE_SetFileDataState)
      {
         this->entryInfoPtr = entryInfo;
         this->dataState = dataState;
      }

      SetFileDataStateMsg() : BaseType(NETMSGTYPE_SetFileDataState) {}

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::backedPtr(obj->entryInfoPtr, obj->entryInfo)
            % obj->dataState;
      }

   private:
      // for serialization
      EntryInfo* entryInfoPtr;
      uint8_t dataState;

      // for deserialization
      EntryInfo entryInfo;

   public:
      EntryInfo* getEntryInfo()
      {
         return &this->entryInfo;
      }

      uint8_t getFileDataState()
      {
         return this->dataState;
      }

      bool supportsMirroring() const { return true; }
};