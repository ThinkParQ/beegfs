#ifndef GETFILEVERSIONMSG_H
#define GETFILEVERSIONMSG_H

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>

class GetFileVersionMsg : public MirroredMessageBase<GetFileVersionMsg>
{
   public:
      GetFileVersionMsg(EntryInfo& entryInfo) :
         BaseType(NETMSGTYPE_GetFileVersion),
         entryInfo(&entryInfo)
      {
      }

      GetFileVersionMsg() : BaseType(NETMSGTYPE_GetFileVersion) {}

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx % serdes::backedPtr(obj->entryInfo, obj->parsed.entryInfo);
      }

      bool supportsMirroring() const { return true; }

   private:
      EntryInfo* entryInfo;

      struct {
         EntryInfo entryInfo;
      } parsed;

   public:
      EntryInfo& getEntryInfo() const { return *entryInfo; }
};

#endif
