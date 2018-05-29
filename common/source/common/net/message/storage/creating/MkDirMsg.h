#ifndef MKDIRMSG_H_
#define MKDIRMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>
#include <common/storage/FileEvent.h>
#include <common/storage/StatData.h>


#define MKDIRMSG_FLAG_NOMIRROR            1 /* do not use mirror setting from parent
                                             * (i.e. do not mirror) */
#define MKDIRMSG_FLAG_HAS_EVENT           4 /* contains file event logging information */

class MkDirMsg : public MirroredMessageBase<MkDirMsg>
{
   friend class AbstractNetMessageFactory;

   public:

      /**
       * @param parentEntryInfo just a reference, so do not free it as long as you use this object!
       * @param preferredNodes just a reference, so do not free it as long as you use this object!
       */
      MkDirMsg(const EntryInfo* parentEntryInfo, const std::string& newDirName,
         const unsigned userID, const unsigned groupID, const int mode, const int umask,
         const UInt16List* preferredNodes)
          : BaseType(NETMSGTYPE_MkDir),
            userID(userID),
            groupID(groupID),
            mode(mode),
            umask(umask),
            newDirName(newDirName.c_str() ),
            newDirNameLen(newDirName.length() ),
            parentEntryInfoPtr(parentEntryInfo),
            preferredNodes(preferredNodes)
      {  }

      /**
       * For deserialization only!
       */
      MkDirMsg() : BaseType(NETMSGTYPE_MkDir) {}

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->userID
            % obj->groupID
            % obj->mode
            % obj->umask
            % serdes::backedPtr(obj->parentEntryInfoPtr, obj->parentEntryInfo);

         if (obj->hasFlag(NetMessageHeader::Flag_BuddyMirrorSecond) )
            ctx
               % serdes::backedPtr(obj->createdEntryInfoPtr, obj->createdEntryInfo)
               % obj->parentTimestamps;

         ctx
            % serdes::rawString(obj->newDirName, obj->newDirNameLen, 4)
            % serdes::backedPtr(obj->preferredNodes, obj->parsed.preferredNodes);

         if (obj->isMsgHeaderFeatureFlagSet(MKDIRMSG_FLAG_HAS_EVENT))
            ctx % obj->fileEvent;
      }

   protected:
      bool supportsMirroring() const { return true; }

      unsigned getSupportedHeaderFeatureFlagsMask() const
      {
         return MKDIRMSG_FLAG_NOMIRROR |
            MKDIRMSG_FLAG_HAS_EVENT;
      }

      void setMode(const int mode, const int umask)
      {
         this->mode = mode;
         this->umask = umask;
      }

      void clearPreferredNodes()
      {
         parsed.preferredNodes.clear();
         preferredNodes = &parsed.preferredNodes;
      }

   private:
      uint32_t userID;
      uint32_t groupID;
      int32_t mode;
      int32_t umask;
      FileEvent fileEvent;

      // serialization / deserialization
      const char* newDirName;
      unsigned newDirNameLen;

      // for serialization
      const EntryInfo* parentEntryInfoPtr; // not owned by this object
      const UInt16List* preferredNodes; // not owned by this object!

      EntryInfo* createdEntryInfoPtr; // not owned by this object; only needed for secondary buddy

      // for deserialization
      EntryInfo parentEntryInfo;
      struct {
         UInt16List preferredNodes;
      } parsed;

      EntryInfo createdEntryInfo; // only needed for secondary buddy

   protected:
      MirroredTimestamps parentTimestamps;

   public:
      const UInt16List& getPreferredNodes() const
      {
         return *preferredNodes;
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

      int getUmask() const
      {
         return umask;
      }

      const EntryInfo* getParentInfo() const
      {
         return &parentEntryInfo;
      }

      const char* getNewDirName() const
      {
         return newDirName;
      }

      EntryInfo* getCreatedEntryInfo()
      {
         return &createdEntryInfo;
      }

      void setCreatedEntryInfo(EntryInfo* createdEntryInfo)
      {
         this->createdEntryInfoPtr = createdEntryInfo;
      }

      const FileEvent* getFileEvent() const
      {
         if (isMsgHeaderFeatureFlagSet(MKDIRMSG_FLAG_HAS_EVENT))
            return &fileEvent;
         else
            return nullptr;
      }
};

#endif /*MKDIRMSG_H_*/
