#ifndef OPENFILEMSG_H_
#define OPENFILEMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/nodes/NumNodeID.h>
#include <common/storage/EntryInfo.h>
#include <common/storage/FileEvent.h>
#include <common/storage/Path.h>
#include <common/storage/StatData.h>

#define OPENFILEMSG_FLAG_USE_QUOTA          1 /* if the message contains quota informations */
#define OPENFILEMSG_FLAG_HAS_EVENT          2 /* contains file event logging information */

class OpenFileMsg : public MirroredMessageBase<OpenFileMsg>
{
   public:

      /**
       * @param enrtyInfo just a reference, so do not free it as long as you use this object!
       * @param accessFlags OPENFILE_ACCESS_... flags
       */
      OpenFileMsg(const NumNodeID clientNumID, const EntryInfo* entryInfo,
         const unsigned accessFlags)
          : BaseType(NETMSGTYPE_OpenFile),
            clientNumID(clientNumID),
            accessFlags(accessFlags),
            entryInfoPtr(entryInfo)
      { }

      /**
       * For deserialization only!
       */
      OpenFileMsg() : BaseType(NETMSGTYPE_OpenFile) { }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->clientNumID
            % obj->accessFlags;

         if(obj->hasFlag(NetMessageHeader::Flag_BuddyMirrorSecond) )
            ctx
               % obj->sessionFileID
               % serdes::rawString(obj->fileHandleID, obj->fileHandleIDLen, 4)
               % obj->fileTimestamps;

         ctx % serdes::backedPtr(obj->entryInfoPtr, obj->entryInfo);

         if (obj->isMsgHeaderFeatureFlagSet(OPENFILEMSG_FLAG_HAS_EVENT))
            ctx % obj->fileEvent;
      }

   private:
      NumNodeID clientNumID;

      uint32_t accessFlags;

      uint32_t sessionFileID; // only needed for secondary buddy; will be set by primary
      const char* fileHandleID; // only needed for secondary buddy; will be set by primary
      unsigned fileHandleIDLen;

      FileEvent fileEvent;

      // serialization
      const EntryInfo* entryInfoPtr;

      // deserialization
      EntryInfo entryInfo;

   protected:
      MirroredTimestamps fileTimestamps;

   public:

      // getters & setters
      NumNodeID getClientNumID() const
      {
         return this->clientNumID;
      }

      EntryInfo* getEntryInfo()
      {
         return &this->entryInfo;
      }

      unsigned getAccessFlags() const
      {
         return this->accessFlags;
      }

      unsigned getSupportedHeaderFeatureFlagsMask() const override
      {
         return OPENFILEMSG_FLAG_USE_QUOTA |
            OPENFILEMSG_FLAG_HAS_EVENT;
      }

      bool supportsMirroring() const override { return true; }

      const char* getFileHandleID() const
      {
         return fileHandleID;
      }

      void setFileHandleID(const char* fileHandleID)
      {
         this->fileHandleID = fileHandleID;
         this->fileHandleIDLen = strlen(fileHandleID);
      }

      unsigned getSessionFileID() const
      {
         return sessionFileID;
      }

      void setSessionFileID(const unsigned sessionFileID)
      {
         this->sessionFileID = sessionFileID;
      }

      const FileEvent* getFileEvent() const
      {
         if (isMsgHeaderFeatureFlagSet(OPENFILEMSG_FLAG_HAS_EVENT))
            return &fileEvent;
         else
            return nullptr;
      }
};

#endif /*OPENFILEMSG_H_*/
