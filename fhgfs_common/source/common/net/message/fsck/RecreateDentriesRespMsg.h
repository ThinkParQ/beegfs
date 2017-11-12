#ifndef RECREATEDENTRIESRESPMSG_H
#define RECREATEDENTRIESRESPMSG_H

#include <common/fsck/FsckDirEntry.h>
#include <common/fsck/FsckFileInode.h>
#include <common/fsck/FsckFsID.h>
#include <common/net/message/NetMessage.h>

class RecreateDentriesRespMsg : public NetMessageSerdes<RecreateDentriesRespMsg>
{
   public:
      RecreateDentriesRespMsg(FsckFsIDList* failedCreates, FsckDirEntryList* createdDentries,
         FsckFileInodeList* createdInodes) :
         BaseType(NETMSGTYPE_RecreateDentriesResp)
      {
         this->failedCreates = failedCreates;
         this->createdDentries = createdDentries;
         this->createdInodes = createdInodes;
      }

      RecreateDentriesRespMsg() : BaseType(NETMSGTYPE_RecreateDentriesResp)
      {
      }

   private:
      FsckFsIDList* failedCreates;
      FsckDirEntryList* createdDentries;
      FsckFileInodeList* createdInodes;

      struct {
         FsckFsIDList failedCreates;
         FsckDirEntryList createdDentries;
         FsckFileInodeList createdInodes;
      } parsed;

   public:
      FsckFsIDList& getFailedCreates()
      {
         return *failedCreates;
      }

      FsckDirEntryList& getCreatedDentries()
      {
         return *createdDentries;
      }

      FsckFileInodeList& getCreatedInodes()
      {
         return *createdInodes;
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::backedPtr(obj->failedCreates, obj->parsed.failedCreates)
            % serdes::backedPtr(obj->createdDentries, obj->parsed.createdDentries)
            % serdes::backedPtr(obj->createdInodes, obj->parsed.createdInodes);
      }
};


#endif /*RECREATEDENTRIESRESPMSG_H*/
