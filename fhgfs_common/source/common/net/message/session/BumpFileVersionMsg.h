#ifndef BUMPFILEVERSIONMSG_H
#define BUMPFILEVERSIONMSG_H

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>
#include <common/storage/FileEvent.h>

#define BUMPFILEVERSIONMSG_FLAG_PERSISTENT 1 /* change persistent file version number too */
#define BUMPFILEVERSIONMSG_FLAG_HASEVENT   2 /* has a loggable event attached */

class BumpFileVersionMsg : public MirroredMessageBase<BumpFileVersionMsg>
{
   public:
      BumpFileVersionMsg(EntryInfo& entryInfo) :
         BaseType(NETMSGTYPE_BumpFileVersion),
         entryInfo(&entryInfo)
      {
      }

      BumpFileVersionMsg() : BaseType(NETMSGTYPE_BumpFileVersion) {}

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx % serdes::backedPtr(obj->entryInfo, obj->parsed.entryInfo);

         if (obj->isMsgHeaderFeatureFlagSet(BUMPFILEVERSIONMSG_FLAG_HASEVENT))
            ctx % obj->fileEvent;
      }

      virtual bool supportsMirroring() const override { return true; }

      virtual unsigned getSupportedHeaderFeatureFlagsMask() const override
      {
         return
            BUMPFILEVERSIONMSG_FLAG_PERSISTENT |
            BUMPFILEVERSIONMSG_FLAG_HASEVENT;
      }

   private:
      EntryInfo* entryInfo;
      FileEvent fileEvent;

      struct {
         EntryInfo entryInfo;
      } parsed;

   public:
      EntryInfo& getEntryInfo() const { return *entryInfo; }

      const FileEvent* getFileEvent() const
      {
         if (isMsgHeaderFeatureFlagSet(BUMPFILEVERSIONMSG_FLAG_HASEVENT))
            return &fileEvent;
         else
            return nullptr;
      }
};

#endif
