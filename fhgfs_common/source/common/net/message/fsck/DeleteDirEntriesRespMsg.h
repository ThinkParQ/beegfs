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

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx % serdes::backedPtr(obj->failedEntries, obj->parsed.failedEntries);
      }
};


#endif /*DELETEDIRENTRIESRESPMSG_H*/
