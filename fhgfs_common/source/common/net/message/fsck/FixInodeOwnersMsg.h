#ifndef FIXINODEOWNERSMSG_H
#define FIXINODEOWNERSMSG_H

#include <common/fsck/FsckDirInode.h>
#include <common/net/message/NetMessage.h>

class FixInodeOwnersMsg : public NetMessageSerdes<FixInodeOwnersMsg>
{
   public:
      FixInodeOwnersMsg(FsckDirInodeList* inodes) : BaseType(NETMSGTYPE_FixInodeOwners)
      {
         this->inodes = inodes;
      }

      FixInodeOwnersMsg(): BaseType(NETMSGTYPE_FixInodeOwners)
      {
      }

   private:
      FsckDirInodeList* inodes;

      // for deserialization
      struct {
         FsckDirInodeList inodes;
      } parsed;

   public:
      FsckDirInodeList& getInodes()
      {
         return *inodes;
      }

      virtual TestingEqualsRes testingEquals(NetMessage* msg)
      {
         FixInodeOwnersMsg* other = (FixInodeOwnersMsg*) msg;
         return *inodes == *other->inodes
            ? TestingEqualsRes_TRUE
            : TestingEqualsRes_FALSE;
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx % serdes::backedPtr(obj->inodes, obj->parsed.inodes);
      }
};


#endif /*FIXINODEOWNERSMSG_H*/
