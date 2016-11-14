#ifndef DELETEDIRENTRIESRESPMSG_H
#define DELETEDIRENTRIESRESPMSG_H

#include <common/fsck/FsckDirEntry.h>
#include <common/net/message/NetMessage.h>

class DeleteDirEntriesRespMsg : public NetMessageSerdes<DeleteDirEntriesRespMsg>
{
   public:
      DeleteDirEntriesRespMsg(FsckDirEntryList* failedEntries) :
         BaseType(NETMSGTYPE_DeleteDirEntriesResp)
      {
         this->failedEntries = failedEntries;
      }

      DeleteDirEntriesRespMsg() : BaseType(NETMSGTYPE_DeleteDirEntriesResp)
      {
      }

   private:
      FsckDirEntryList* failedEntries;

      struct {
         FsckDirEntryList failedEntries;
      } parsed;

   public:
      FsckDirEntryList& getFailedEntries()
      {
         return *failedEntries;
      }

      virtual TestingEqualsRes testingEquals(NetMessage* msg)
      {
         DeleteDirEntriesRespMsg* other = (DeleteDirEntriesRespMsg*) msg;
         return *failedEntries == *other->failedEntries
            ? TestingEqualsRes_TRUE
            : TestingEqualsRes_FALSE;
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx % serdes::backedPtr(obj->failedEntries, obj->parsed.failedEntries);
      }
};


#endif /*DELETEDIRENTRIESRESPMSG_H*/
