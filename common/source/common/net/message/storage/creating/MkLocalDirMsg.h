#ifndef MKLOCALDIRMSG_H_
#define MKLOCALDIRMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/nodes/NumNodeID.h>
#include <common/storage/striping/StripePattern.h>
#include <common/storage/EntryInfo.h>
#include <common/storage/StatData.h>


class MkLocalDirMsg : public MirroredMessageBase<MkLocalDirMsg>
{
   friend class AbstractNetMessageFactory;
   friend class TestSerialization;

   public:

      /**
       * @param entryInfo just a reference, so do not free it as long as you use this object!
       */
      MkLocalDirMsg(EntryInfo* entryInfo, unsigned userID, unsigned groupID, int mode,
         StripePattern* pattern, NumNodeID parentNodeID,
         const CharVector& defaultACLXAttr, const CharVector& accessACLXAttr) :
            BaseType(NETMSGTYPE_MkLocalDir),
            defaultACLXAttr(defaultACLXAttr), accessACLXAttr(accessACLXAttr)
      {
         this->entryInfoPtr = entryInfo;
         this->userID = userID;
         this->groupID = groupID;
         this->mode = mode;
         this->pattern = pattern;
         this->parentNodeID = parentNodeID;
      }

      /**
       * For deserialization only!
       */
      MkLocalDirMsg() : BaseType(NETMSGTYPE_MkLocalDir) {}

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->userID
            % obj->groupID
            % obj->mode
            % serdes::backedPtr(obj->entryInfoPtr, obj->entryInfo)
            % serdes::backedPtr(obj->pattern, obj->parsed.pattern)
            % obj->parentNodeID
            % obj->defaultACLXAttr
            % obj->accessACLXAttr;

         if (obj->hasFlag(NetMessageHeader::Flag_BuddyMirrorSecond))
            ctx % obj->dirTimestamps;
      }

      bool supportsMirroring() const { return true; }

   private:
      uint32_t userID;
      uint32_t groupID;
      int32_t mode;
      NumNodeID parentNodeID;

      // for serialization
      EntryInfo* entryInfoPtr; // not owned by this object!
      StripePattern* pattern;  // not owned by this object!

      // for deserialization
      EntryInfo entryInfo;
      struct {
         std::unique_ptr<StripePattern> pattern;
      } parsed;

      // ACLs
      CharVector defaultACLXAttr;
      CharVector accessACLXAttr;

   protected:
      MirroredTimestamps dirTimestamps;

   public:
      StripePattern& getPattern()
      {
         return *pattern;
      }

      void setPattern(StripePattern* pattern)
      {
         this->pattern = pattern;
      }

      // getters & setters
      unsigned getUserID() const
      {
         return userID;
      }

      unsigned getGroupID() const
      {
         return groupID;
      }

      int getMode() const
      {
         return mode;
      }

      NumNodeID getParentNodeID() const
      {
         return this->parentNodeID;
      }

      EntryInfo* getEntryInfo()
      {
         return this->entryInfoPtr;
      }

      const CharVector& getDefaultACLXAttr() const
      {
         return this->defaultACLXAttr;
      }

      const CharVector& getAccessACLXAttr() const
      {
         return this->accessACLXAttr;
      }

      void setDirTimestamps(const MirroredTimestamps& ts) { dirTimestamps = ts; }
};

#endif /*MKLOCALDIRMSG_H_*/
