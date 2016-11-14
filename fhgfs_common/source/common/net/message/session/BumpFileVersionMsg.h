#ifndef BUMPFILEVERSIONMSG_H
#define BUMPFILEVERSIONMSG_H

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>

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
      }

      virtual bool supportsMirroring() const override { return true; }

   private:
      EntryInfo* entryInfo;

      struct {
         EntryInfo entryInfo;
      } parsed;

   public:
      EntryInfo& getEntryInfo() const { return *entryInfo; }
};

#endif
