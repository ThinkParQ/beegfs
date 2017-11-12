#ifndef LINKTOLOSTANDFOUNDRESPMSG_H
#define LINKTOLOSTANDFOUNDRESPMSG_H

#include <common/fsck/FsckDirInode.h>
#include <common/net/message/NetMessage.h>
#include <common/toolkit/FsckTk.h>

class LinkToLostAndFoundRespMsg : public NetMessageSerdes<LinkToLostAndFoundRespMsg>
{
   public:
      LinkToLostAndFoundRespMsg(FsckDirInodeList* failedDirInodes,
         FsckDirEntryList* createdDentries) : BaseType(NETMSGTYPE_LinkToLostAndFoundResp)
      {
         this->failedDirInodes = failedDirInodes;
         this->failedFileInodes = NULL;
         this->createdDentries = createdDentries;
         this->entryType = FsckDirEntryType_DIRECTORY;
      }

      LinkToLostAndFoundRespMsg(FsckFileInodeList* failedFileInodes,
         FsckDirEntryList* createdDentries): BaseType(NETMSGTYPE_LinkToLostAndFoundResp)
      {
         this->failedFileInodes = failedFileInodes;
         this->failedDirInodes = NULL;
         this->createdDentries = createdDentries;
         this->entryType = FsckDirEntryType_REGULARFILE;
      }

      LinkToLostAndFoundRespMsg() : BaseType(NETMSGTYPE_LinkToLostAndFoundResp)
      {
      }

   private:
      FsckDirEntryType entryType; // to indicate, whether dir inodes or file inodes should be
                                  // linked; important for serialization

      FsckDirInodeList* failedDirInodes;
      FsckFileInodeList* failedFileInodes;

      FsckDirEntryList* createdDentries;

      // for deserialization
      struct {
         FsckDirInodeList failedDirInodes;
         FsckFileInodeList failedFileInodes;
         FsckDirEntryList createdDentries;
      } parsed;

   public:
      FsckDirInodeList& getFailedDirInodes()
      {
         return *failedDirInodes;
      }

      FsckFileInodeList& getFailedFileInodes()
      {
         return *failedFileInodes;
      }

      FsckDirEntryList& getCreatedDirEntries()
      {
         return *createdDentries;
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx % serdes::as<int32_t>(obj->entryType);

         if (FsckDirEntryType_ISDIR(obj->entryType))
            ctx % serdes::backedPtr(obj->failedDirInodes, obj->parsed.failedDirInodes);
         else
            ctx % serdes::backedPtr(obj->failedFileInodes, obj->parsed.failedFileInodes);

         ctx % serdes::backedPtr(obj->createdDentries, obj->parsed.createdDentries);
      }
};


#endif /*LINKTOLOSTANDFOUNDRESPMSG_H*/
