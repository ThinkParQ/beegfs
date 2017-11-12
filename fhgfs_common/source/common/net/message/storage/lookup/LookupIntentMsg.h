#ifndef LOOKUPINTENTMSG_H_
#define LOOKUPINTENTMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>
#include <common/storage/FileEvent.h>
#include <common/storage/StatData.h>
#include <common/storage/striping/StripePattern.h>


/* intentFlags as payload
   Keep these flags in sync with the client msg flags:
   fhgfs_client_mode/source/closed/net/message/storage/LookupIntentMsg.h */

#define LOOKUPINTENTMSG_FLAG_REVALIDATE         1 /* revalidate entry, cancel if invalid */
#define LOOKUPINTENTMSG_FLAG_CREATE             2 /* create file */
#define LOOKUPINTENTMSG_FLAG_CREATEEXCLUSIVE    4 /* exclusive file creation */
#define LOOKUPINTENTMSG_FLAG_OPEN               8 /* open file */
#define LOOKUPINTENTMSG_FLAG_STAT              16 /* stat file */


// feature flags as header flags
#define LOOKUPINTENTMSG_FLAG_USE_QUOTA          1 /* if the message contains quota information */
#define LOOKUPINTENTMSG_FLAG_HAS_EVENT          8 /* contains file event logging information */

class LookupIntentMsg : public MirroredMessageBase<LookupIntentMsg>
{
   friend class AbstractNetMessageFactory;

   public:

      /**
       * This just prepares the basic lookup. Use the additional addIntent...() methods to do
       * more than just the lookup.
       *
       * @param parentInfo just a reference, so do not free it as long as you use this object!
       * @param entryName
       */
      LookupIntentMsg(const EntryInfo* parentInfo, const std::string& entryName) :
         BaseType(NETMSGTYPE_LookupIntent)
      {
         this->intentFlags = 0;

         this->parentInfoPtr = parentInfo;
         this->entryName = entryName;
      }

      /**
       * Used for revalidate intent
       * @param parentInfo just a reference, so do not free it as long as you use this object!
       * @param entryInfo  just a reference, so do not free it as long as you use this object!
       *
       */
      LookupIntentMsg(const EntryInfo* parentInfo, const EntryInfo* entryInfo) :
         BaseType(NETMSGTYPE_LookupIntent)
      {
         this->intentFlags = LOOKUPINTENTMSG_FLAG_REVALIDATE;

         this->parentInfoPtr = parentInfo;
         this->entryInfoPtr  = entryInfo;
      }

      LookupIntentMsg() : BaseType(NETMSGTYPE_LookupIntent)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->intentFlags
            % serdes::backedPtr(obj->parentInfoPtr, obj->parentInfo)
            % serdes::stringAlign4(obj->entryName);

         if(obj->intentFlags & LOOKUPINTENTMSG_FLAG_REVALIDATE)
            ctx % serdes::backedPtr(obj->entryInfoPtr, obj->entryInfo);

         if(obj->intentFlags & LOOKUPINTENTMSG_FLAG_OPEN)
            ctx
               % obj->accessFlags
               % obj->sessionID;

         if(obj->intentFlags & LOOKUPINTENTMSG_FLAG_CREATE)
         {
            ctx
               % obj->userID
               % obj->groupID
               % obj->mode
               % obj->umask
               % serdes::backedPtr(obj->preferredTargets, obj->parsed.preferredTargets);

            if (obj->isMsgHeaderFeatureFlagSet(LOOKUPINTENTMSG_FLAG_HAS_EVENT))
               ctx % obj->fileEvent;

            if (obj->hasFlag(NetMessageHeader::Flag_BuddyMirrorSecond))
               ctx
                  % obj->newEntryID
                  % serdes::backedPtr(obj->newStripePattern, obj->parsed.newStripePattern)
                  % obj->newOwnerFD
                  % obj->dirTimestamps;
         }

         if (obj->hasFlag(NetMessageHeader::Flag_BuddyMirrorSecond))
            ctx
               % obj->fileTimestamps;
      }

      bool supportsMirroring() const override { return true; }

   private:
      int32_t intentFlags; // combination of LOOKUPINTENTMSG_FLAG_...

      // on revalidate retrieved from entryInfo in deserialization method
      std::string entryName; // (lookup data)

      uint32_t userID; // (file creation data)
      uint32_t groupID; // (file creation data)
      int32_t mode; // file creation mode permission bits  (file creation data)
      int32_t umask; // umask of context (file creation data)
      FileEvent fileEvent;

      NumNodeID sessionID; // (file open data)
      uint32_t accessFlags; // OPENFILE_ACCESS_... flags (file open data)

      // for serialization
      const UInt16List* preferredTargets; // not owned by this object! (file creation data)
      const EntryInfo* parentInfoPtr;
      const EntryInfo* entryInfoPtr; // only set on revalidate

      std::string newEntryID;
      StripePattern* newStripePattern;

      // for deserialization
      EntryInfo parentInfo;
      EntryInfo entryInfo;               // (revalidate data)
      struct {
         UInt16List preferredTargets;
         std::unique_ptr<StripePattern> newStripePattern;
      } parsed;

   protected:
      unsigned newOwnerFD;
      MirroredTimestamps dirTimestamps;
      MirroredTimestamps fileTimestamps;

   public:
      void addIntentCreate(const unsigned userID, const unsigned groupID, const int mode,
         const int umask, const UInt16List* preferredTargets)
      {
         this->intentFlags |= LOOKUPINTENTMSG_FLAG_CREATE;

         this->userID = userID;
         this->groupID = groupID;
         this->mode = mode;
         this->umask = umask;
         this->preferredTargets = preferredTargets;
      }

      void addBuddyInfo(std::string newEntryID, StripePattern* newStripePattern)
      {
         this->newEntryID = std::move(newEntryID);
         this->newStripePattern = newStripePattern;
      }

      void addIntentCreateExclusive()
      {
         this->intentFlags |= LOOKUPINTENTMSG_FLAG_CREATEEXCLUSIVE;
      }

      /**
       * @param accessFlags OPENFILE_ACCESS_... flags
       */
      void addIntentOpen(const NumNodeID sessionID, const unsigned accessFlags)
      {
         this->intentFlags |= LOOKUPINTENTMSG_FLAG_OPEN;

         this->sessionID = sessionID;

         this->accessFlags = accessFlags;
      }

      void addIntentStat()
      {
         this->intentFlags |= LOOKUPINTENTMSG_FLAG_STAT;
      }

      const UInt16List& getPreferredTargets()
      {
         return *preferredTargets;
      }

      // getters & setters

      int getIntentFlags() const
      {
         return this->intentFlags;
      }

      std::string getEntryName()
      {
         return this->entryName;
      }


      EntryInfo* getParentInfo()
      {
         return &this->parentInfo;
      }

      EntryInfo* getEntryInfo()
      {
         return &this->entryInfo;
      }

      unsigned getUserID() const
      {
         return this->userID;
      }

      unsigned getGroupID() const
      {
         return this->groupID;
      }

      int getMode() const
      {
         return mode;
      }

      int getUmask() const
      {
         return umask;
      }

      NumNodeID getSessionID() const
      {
         return sessionID;
      }

      unsigned getAccessFlags() const
      {
         return accessFlags;
      }

      const FileEvent* getFileEvent() const
      {
         if (isMsgHeaderFeatureFlagSet(LOOKUPINTENTMSG_FLAG_HAS_EVENT))
            return &fileEvent;
         else
            return nullptr;
      }

      unsigned getSupportedHeaderFeatureFlagsMask() const override
      {
         return LOOKUPINTENTMSG_FLAG_USE_QUOTA |
            LOOKUPINTENTMSG_FLAG_HAS_EVENT;
      }

      const std::string& getNewEntryID() const { return newEntryID; }
      StripePattern* getNewStripePattern() const { return newStripePattern; }
};

#endif /* LOOKUPINTENTMSG_H_ */
