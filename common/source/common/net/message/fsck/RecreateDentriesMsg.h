#ifndef RECREATEDENTRIESMSG_H
#define RECREATEDENTRIESMSG_H

#include <common/fsck/FsckFsID.h>
#include <common/net/message/NetMessage.h>

class RecreateDentriesMsg : public NetMessageSerdes<RecreateDentriesMsg>
{
   public:
      RecreateDentriesMsg(FsckFsIDList* fsIDs) : BaseType(NETMSGTYPE_RecreateDentries)
      {
         this->fsIDs = fsIDs;
      }

      RecreateDentriesMsg() : BaseType(NETMSGTYPE_RecreateDentries)
      {
      }

   private:
      FsckFsIDList* fsIDs;

      struct {
         FsckFsIDList fsIDs;
      } parsed;

   public:
      FsckFsIDList& getFsIDs()
      {
         return *fsIDs;
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx % serdes::backedPtr(obj->fsIDs, obj->parsed.fsIDs);
      }
};


#endif /*RECREATEDENTRIESMSG_H*/
