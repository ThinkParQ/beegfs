#ifndef FIXINODEOWNERSINDENTRYMSG_H
#define FIXINODEOWNERSINDENTRYMSG_H

#include <common/fsck/FsckDirEntry.h>
#include <common/net/message/NetMessage.h>

class FixInodeOwnersInDentryMsg : public NetMessageSerdes<FixInodeOwnersInDentryMsg>
{
   public:
      FixInodeOwnersInDentryMsg(FsckDirEntryList& dentries, NumNodeIDList& owners)
         : BaseType(NETMSGTYPE_FixInodeOwnersInDentry),
           dentries(&dentries),
           owners(&owners)
      {
      }

      FixInodeOwnersInDentryMsg(): BaseType(NETMSGTYPE_FixInodeOwnersInDentry)
      {
      }

   private:
      FsckDirEntryList* dentries;
      NumNodeIDList* owners;

      struct {
         FsckDirEntryList dentries;
         NumNodeIDList owners;
      } parsed;

   public:
      FsckDirEntryList& getDentries()
      {
         return *dentries;
      }

      NumNodeIDList& getOwners()
      {
         return *owners;
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::backedPtr(obj->dentries, obj->parsed.dentries)
            % serdes::backedPtr(obj->owners, obj->parsed.owners);
      }
};


#endif /*FIXINODEOWNERSINDENTRYMSG_H*/
