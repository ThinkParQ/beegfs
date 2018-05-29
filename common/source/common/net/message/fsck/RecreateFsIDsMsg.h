#ifndef RECREATEFSIDSMSG_H
#define RECREATEFSIDSMSG_H

#include <common/fsck/FsckDirEntry.h>
#include <common/net/message/NetMessage.h>

class RecreateFsIDsMsg : public NetMessageSerdes<RecreateFsIDsMsg>
{
   public:
      RecreateFsIDsMsg(FsckDirEntryList* entries) : BaseType(NETMSGTYPE_RecreateFsIDs)
      {
         this->entries = entries;
      }

      RecreateFsIDsMsg() : BaseType(NETMSGTYPE_RecreateFsIDs)
      {
      }

   private:
      FsckDirEntryList* entries;

      struct {
         FsckDirEntryList entries;
      } parsed;

   public:
      FsckDirEntryList& getEntries()
      {
         return *entries;
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx % serdes::backedPtr(obj->entries, obj->parsed.entries);
      }
};


#endif /*RECREATEFSIDSMSG_H*/
