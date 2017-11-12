#ifndef UPDATEDIRATTRIBSMSG_H
#define UPDATEDIRATTRIBSMSG_H

#include <common/fsck/FsckDirInode.h>
#include <common/net/message/NetMessage.h>

class UpdateDirAttribsMsg : public NetMessageSerdes<UpdateDirAttribsMsg>
{
   public:
      UpdateDirAttribsMsg(FsckDirInodeList* inodes) :
         BaseType(NETMSGTYPE_UpdateDirAttribs)
      {
         this->inodes = inodes;
      }

      UpdateDirAttribsMsg() : BaseType(NETMSGTYPE_UpdateDirAttribs)
      {
      }

   private:
      FsckDirInodeList* inodes;

      struct {
         FsckDirInodeList inodes;
      } parsed;

   public:
      FsckDirInodeList& getInodes()
      {
         return *inodes;
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::backedPtr(obj->inodes, obj->parsed.inodes);
      }
};


#endif /*UPDATEDIRATTRIBSMSG_H*/
