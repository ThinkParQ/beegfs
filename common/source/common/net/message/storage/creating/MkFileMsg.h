#ifndef MKFILEMSG_H_
#define MKFILEMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>
#include <common/storage/FileEvent.h>
#include <common/storage/StatData.h>
#include <common/storage/striping/StripePattern.h>


#define MKFILEMSG_FLAG_STRIPEHINTS        1 /* msg contains extra stripe hints */
#define MKFILEMSG_FLAG_STORAGEPOOLID      2 /* msg contains a storage pool ID to override parent */
#define MKFILEMSG_FLAG_HAS_EVENT          4 /* contains file event logging information */

class MkFileMsg : public MirroredMessageBase<MkFileMsg>
{
   public:

      /**
       * @param entryInfo just a reference, so do not free it as long as you use this object!
       */
      MkFileMsg(const EntryInfo* parentInfo, const std::string& newName, const unsigned userID,
         const unsigned groupID, const int mode, const int umask,
         const UInt16List* preferredTargets)
          : BaseType(NETMSGTYPE_MkFile),
            newName(newName.c_str() ),
            newNameLen(newName.length() ),
            userID(userID),
            groupID(groupID),
            mode(mode),
            umask(umask),
            parentInfoPtr(parentInfo),
            preferredTargets(preferredTargets)
      { }

      /**
       * For deserialization only!
       */
      MkFileMsg() : BaseType(NETMSGTYPE_MkFile) {}

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->userID
            % obj->groupID
            % obj->mode
            % obj->umask;

         if(obj->isMsgHeaderFeatureFlagSet(MKFILEMSG_FLAG_STRIPEHINTS) )
         {
            ctx
               % obj->numtargets
               % obj->chunksize;
         }

         if(obj->isMsgHeaderFeatureFlagSet(MKFILEMSG_FLAG_STORAGEPOOLID) )
         {
            ctx
               % obj->storagePoolId;
         }

         ctx
            % serdes::backedPtr(obj->parentInfoPtr, obj->parentInfo)
            % serdes::rawString(obj->newName, obj->newNameLen, 4);

         if(obj->hasFlag(NetMessageHeader::Flag_BuddyMirrorSecond))
            ctx
               % serdes::rawString(obj->newEntryID, obj->newEntryIDLen, 4)
               % serdes::backedPtr(obj->pattern, obj->parsed.pattern)
               % obj->dirTimestamps
               % obj->createTime;

         ctx % serdes::backedPtr(obj->preferredTargets, obj->parsed.preferredTargets);

         if (obj->isMsgHeaderFeatureFlagSet(MKFILEMSG_FLAG_HAS_EVENT))
            ctx % obj->fileEvent;
      }

   protected:
      MirroredTimestamps dirTimestamps;
      int64_t createTime;

      const char* newName;
      unsigned newNameLen;
      uint32_t userID;
      uint32_t groupID;
      int32_t mode;
      int32_t umask;

      uint32_t numtargets;
      uint32_t chunksize;
      FileEvent fileEvent;

      // if this is set, the storage pool of the parent directory is ignored and this pool is used
      // instead
      StoragePoolId storagePoolId;

      // secondary needs to know the created entryID, because it needs to use the same ID; it also
      // needs to use exactly the same stripe pattern
      // will only be set and used if NetMessageHeader::Flag_BuddyMirrorSecond is set
      const char* newEntryID;
      unsigned newEntryIDLen;
      StripePattern* pattern; // not owned by this object!

      bool supportsMirroring() const { return true; }

      unsigned getSupportedHeaderFeatureFlagsMask() const
      {
         return MKFILEMSG_FLAG_STRIPEHINTS |
                MKFILEMSG_FLAG_STORAGEPOOLID |
                MKFILEMSG_FLAG_HAS_EVENT;
      }

   private:
      // for serialization
      const EntryInfo* parentInfoPtr;
      const UInt16List* preferredTargets; // not owned by this object!

      // for deserialization
      EntryInfo parentInfo;
      struct {
         UInt16List preferredTargets;
         std::unique_ptr<StripePattern> pattern;
      } parsed;

   public:
      const UInt16List& getPreferredNodes()
      {
         return *preferredTargets;
      }

      // getters & setters
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
         return this->mode;
      }

      int getUmask() const
      {
         return this->umask;
      }

      const char* getNewName() const
      {
         return this->newName;
      }

      const EntryInfo* getParentInfo() const
      {
         return &this->parentInfo;
      }

      /**
       * Note: Adds MKFILEMSG_FLAG_STRIPEHINTS.
       */
      void setStripeHints(unsigned numtargets, unsigned chunksize)
      {
         addMsgHeaderFeatureFlag(MKFILEMSG_FLAG_STRIPEHINTS);

         this->numtargets = numtargets;
         this->chunksize = chunksize;
      }

      /**
       * @return 0 if MKFILEMSG_FLAG_STRIPEHINTS feature flag is not set.
       */
      unsigned getNumTargets() const
      {
         return isMsgHeaderFeatureFlagSet(MKFILEMSG_FLAG_STRIPEHINTS) ? numtargets : 0;
      }

      /**
       * @return 0 if MKFILEMSG_FLAG_STRIPEHINTS feature flag is not set.
       */
      unsigned getChunkSize() const
      {
         return isMsgHeaderFeatureFlagSet(MKFILEMSG_FLAG_STRIPEHINTS) ? chunksize : 0;
      }

      void setNewEntryID(const char* newEntryID)
      {
         this->newEntryID = newEntryID;
         this->newEntryIDLen = strlen(newEntryID);
      }

      const char* getNewEntryID()
      {
         return this->newEntryID;
      }

      void setPattern(StripePattern* pattern)
      {
         this->pattern = pattern;
      }

      StripePattern& getPattern()
      {
         return *pattern;
      }

      void setDirTimestamps(MirroredTimestamps ts) { dirTimestamps = ts; }
      void setCreateTime(int64_t ts) { createTime = ts; }

      const FileEvent* getFileEvent() const
      {
         if (isMsgHeaderFeatureFlagSet(MKFILEMSG_FLAG_HAS_EVENT))
            return &fileEvent;
         else
            return nullptr;
      }
};

#endif /*MKFILEMSG_H_*/
