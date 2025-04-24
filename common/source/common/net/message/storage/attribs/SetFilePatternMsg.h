#pragma once

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>
#include <common/storage/RemoteStorageTarget.h>

class SetFilePatternMsg : public MirroredMessageBase<SetFilePatternMsg>
{
   public:
      /**
       * @param entryInfo just a reference, so do not free it as long as you use this object!
       * @param rst Remote storage targets to be set on file's inode
       */
      SetFilePatternMsg(EntryInfo* entryInfo, RemoteStorageTarget* rst) :
         BaseType(NETMSGTYPE_SetFilePattern)
      {
         this->entryInfoPtr = entryInfo;
         this->rstPtr = rst;
      }

      /**
       * For deserialization only
       */
      SetFilePatternMsg() : BaseType(NETMSGTYPE_SetFilePattern) {}

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::backedPtr(obj->entryInfoPtr, obj->entryInfo)
            % serdes::backedPtr(obj->rstPtr, obj->rst);
      }

   private:
      // for serialization
      EntryInfo* entryInfoPtr;
      RemoteStorageTarget* rstPtr;  // not owned by this object!

      // for deserialization
      EntryInfo entryInfo;
      RemoteStorageTarget rst;

   public:
      EntryInfo* getEntryInfo()
      {
         return &this->entryInfo;
      }

      RemoteStorageTarget* getRemoteStorageTarget()
      {
         return &this->rst;
      }

      bool supportsMirroring() const { return true; }
};
