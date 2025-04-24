#pragma once

#include <common/net/message/NetMessage.h>
#include <common/storage/striping/StripePattern.h>
#include <common/storage/EntryInfo.h>
#include <common/storage/RemoteStorageTarget.h>
#include <common/Common.h>

class SetDirPatternMsg : public MirroredMessageBase<SetDirPatternMsg>
{
   public:
      struct Flags
      {
         static const uint32_t HAS_UID = 1;
      };

      /**
       * @param path just a reference, so do not free it as long as you use this object!
       * @param pattern just a reference, so do not free it as long as you use this object!
       */
      SetDirPatternMsg(EntryInfo* entryInfo, StripePattern* pattern, RemoteStorageTarget* rst) :
         BaseType(NETMSGTYPE_SetDirPattern)
      {
         this->entryInfoPtr = entryInfo;
         this->pattern = pattern;
         this->rstPtr = rst;
      }

      /**
       * For deserialization only
       */
      SetDirPatternMsg() : BaseType(NETMSGTYPE_SetDirPattern)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::backedPtr(obj->entryInfoPtr, obj->entryInfo)
            % serdes::backedPtr(obj->pattern, obj->parsed.pattern)
            % serdes::backedPtr(obj->rstPtr, obj->rst);

         if (obj->isMsgHeaderFeatureFlagSet(Flags::HAS_UID))
            ctx % obj->uid;
      }

      unsigned getSupportedHeaderFeatureFlagsMask() const
      {
         return Flags::HAS_UID;
      }

   private:
      uint32_t uid;

      // for serialization
      EntryInfo* entryInfoPtr;

      StripePattern* pattern;       // not owned by this object!
      RemoteStorageTarget* rstPtr;  // not owned by this object!

      // for deserialization
      EntryInfo entryInfo;
      RemoteStorageTarget rst;
      struct {
         std::unique_ptr<StripePattern> pattern;
      } parsed;

   public:
      StripePattern& getPattern()
      {
         return *pattern;
      }

      EntryInfo* getEntryInfo()
      {
         return &this->entryInfo;
      }

      RemoteStorageTarget* getRemoteStorageTarget()
      {
         return &this->rst;
      }

      uint32_t getUID() const { return uid; }

      void setUID(uint32_t uid)
      {
         setMsgHeaderFeatureFlags(Flags::HAS_UID);
         this->uid = uid;
      }

      bool supportsMirroring() const { return true; }
};


