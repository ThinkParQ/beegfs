#ifndef UPDATEFILEATTRIBSMSG_H
#define UPDATEFILEATTRIBSMSG_H

#include <common/fsck/FsckFileInode.h>
#include <common/net/message/NetMessage.h>

class UpdateFileAttribsMsg : public NetMessageSerdes<UpdateFileAttribsMsg>
{
   public:
      UpdateFileAttribsMsg(FsckFileInodeList* inodes) :
         BaseType(NETMSGTYPE_UpdateFileAttribs)
      {
         this->inodes = inodes;
      }

      UpdateFileAttribsMsg() : BaseType(NETMSGTYPE_UpdateFileAttribs)
      {
      }

   private:
      FsckFileInodeList* inodes;

      struct {
         FsckFileInodeList inodes;
      } parsed;

   public:
      FsckFileInodeList& getInodes()
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


#endif /*UPDATEFILEATTRIBSMSG_H*/
