#ifndef DELETEDIRENTRIESMSG_H
#define DELETEDIRENTRIESMSG_H

#include <common/fsck/FsckDirEntry.h>
#include <common/net/message/NetMessage.h>

class DeleteDirEntriesMsg : public NetMessageSerdes<DeleteDirEntriesMsg>
{
   public:
      DeleteDirEntriesMsg(FsckDirEntryList* entries) : BaseType(NETMSGTYPE_DeleteDirEntries)
      {
         this->entries = entries;
      }

      DeleteDirEntriesMsg() : BaseType(NETMSGTYPE_DeleteDirEntries)
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

      virtual TestingEqualsRes testingEquals(NetMessage* msg)
      {
         DeleteDirEntriesMsg* other = (DeleteDirEntriesMsg*) msg;
         return *entries == *other->entries
            ? TestingEqualsRes_TRUE
            : TestingEqualsRes_FALSE;
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx % serdes::backedPtr(obj->entries, obj->parsed.entries);
      }
};


#endif /*DELETEDIRENTRIESMSG_H*/
