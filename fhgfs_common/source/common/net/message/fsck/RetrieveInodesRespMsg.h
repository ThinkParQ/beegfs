#ifndef RETRIEVEINODESRESPMSG_H
#define RETRIEVEINODESRESPMSG_H

#include <common/fsck/FsckDirInode.h>
#include <common/fsck/FsckFileInode.h>
#include <common/net/message/NetMessage.h>
#include <common/toolkit/ListTk.h>

class RetrieveInodesRespMsg : public NetMessageSerdes<RetrieveInodesRespMsg>
{
   public:
      RetrieveInodesRespMsg(FsckFileInodeList *fileInodes, FsckDirInodeList *dirInodes,
         int64_t lastOffset)  : BaseType(NETMSGTYPE_RetrieveInodesResp)
      {
         this->fileInodes = fileInodes;
         this->dirInodes = dirInodes;
         this->lastOffset = lastOffset;

      }

      RetrieveInodesRespMsg() : BaseType(NETMSGTYPE_RetrieveInodesResp)
      {
      }

   private:
      FsckFileInodeList* fileInodes;
      FsckDirInodeList* dirInodes;
      int64_t lastOffset;

      // for deserialization
      struct {
         FsckFileInodeList fileInodes;
         FsckDirInodeList dirInodes;
      } parsed;

   public:
      FsckFileInodeList& getFileInodes()
      {
         return *fileInodes;
      }

      FsckDirInodeList& getDirInodes()
      {
         return *dirInodes;
      }

      int64_t getLastOffset()
      {
         return lastOffset;
      }

      virtual TestingEqualsRes testingEquals(NetMessage* msg)
      {
         RetrieveInodesRespMsg* msgIn = (RetrieveInodesRespMsg*) msg;

         if (! ListTk::listsEqual(this->fileInodes, &msgIn->getFileInodes() ) )
            return TestingEqualsRes_FALSE;

         if (! ListTk::listsEqual(this->dirInodes, &msgIn->getDirInodes() ) )
            return TestingEqualsRes_FALSE;

         if ( this->lastOffset != msgIn->getLastOffset() )
            return TestingEqualsRes_FALSE;

         return TestingEqualsRes_TRUE;
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::backedPtr(obj->fileInodes, obj->parsed.fileInodes)
            % serdes::backedPtr(obj->dirInodes, obj->parsed.dirInodes)
            % obj->lastOffset;
      }
};


#endif /*RETRIEVEINODESRESPMSG_H*/
