#ifndef CHECKANDREPAIRDUPINODEMSG_H
#define CHECKANDREPAIRDUPINODEMSG_H

#include <common/net/message/NetMessage.h>
#include <common/fsck/FsckDuplicateInodeInfo.h>

class CheckAndRepairDupInodeMsg : public NetMessageSerdes<CheckAndRepairDupInodeMsg>
{
   public:
      CheckAndRepairDupInodeMsg(FsckDuplicateInodeInfoVector* items):
         BaseType(NETMSGTYPE_CheckAndRepairDupInode), dupInodes(items)
      {
      }

      CheckAndRepairDupInodeMsg() : BaseType(NETMSGTYPE_CheckAndRepairDupInode)
      {
      }

   protected:

      // not owned by this object
      FsckDuplicateInodeInfoVector* dupInodes;

      // for deserialization
      struct
      {
         FsckDuplicateInodeInfoVector dupInodes;
      } parsed;

   public:
      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx % serdes::backedPtr(obj->dupInodes, obj->parsed.dupInodes);
      }

      FsckDuplicateInodeInfoVector& getDuplicateInodes()
      {
         return *dupInodes;
      }
};

#endif /* CHECKANDREPAIRDUPINODEMSG_H */
