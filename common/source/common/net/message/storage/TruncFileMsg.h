#ifndef TRUNCFILEMSG_H_
#define TRUNCFILEMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>
#include <common/storage/FileEvent.h>
#include <common/storage/Path.h>
#include <common/storage/StatData.h>


#define TRUNCFILEMSG_FLAG_USE_QUOTA       1 /* if the message contains quota informations */
#define TRUNCFILEMSG_FLAG_HAS_EVENT       2 /* contains file event logging information */


class TruncFileMsg : public MirroredMessageBase<TruncFileMsg>
{
   public:

      /**
       * @param entryID just a reference, so do not free it as long as you use this object!
       */
      TruncFileMsg(int64_t filesize, EntryInfo* entryInfo) :
         BaseType(NETMSGTYPE_TruncFile)
      {
         this->filesize = filesize;

         this->entryInfoPtr = entryInfo;
      }

      TruncFileMsg() : BaseType(NETMSGTYPE_TruncFile) { }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->filesize
            % serdes::backedPtr(obj->entryInfoPtr, obj->entryInfo);

         if (obj->hasFlag(NetMessageHeader::Flag_BuddyMirrorSecond))
            ctx
               % obj->dynAttribs
               % obj->mirroredTimestamps;

         if (obj->isMsgHeaderFeatureFlagSet(TRUNCFILEMSG_FLAG_HAS_EVENT))
            ctx % obj->fileEvent;
      }

      bool supportsMirroring() const { return true; }

   private:
      int64_t filesize;
      FileEvent fileEvent;

      // for serialization
      EntryInfo* entryInfoPtr; // not owned by this object

      // for deserialization
      EntryInfo entryInfo;

   protected:
      DynamicFileAttribsVec dynAttribs;
      MirroredTimestamps mirroredTimestamps;

   public:

      // getters & setters
      int64_t getFilesize()
      {
         return filesize;
      }

      EntryInfo* getEntryInfo()
      {
         return &this->entryInfo;
      }

      const FileEvent* getFileEvent() const
      {
         if (isMsgHeaderFeatureFlagSet(TRUNCFILEMSG_FLAG_HAS_EVENT))
            return &fileEvent;
         else
            return nullptr;
      }

      unsigned getSupportedHeaderFeatureFlagsMask() const
      {
         return TRUNCFILEMSG_FLAG_USE_QUOTA |
            TRUNCFILEMSG_FLAG_HAS_EVENT;
      }
};

#endif /*TRUNCFILEMSG_H_*/
