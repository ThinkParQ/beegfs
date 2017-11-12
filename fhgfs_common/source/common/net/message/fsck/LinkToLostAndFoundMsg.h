#ifndef LINKTOLOSTANDFOUNDMSG_H
#define LINKTOLOSTANDFOUNDMSG_H

#include <common/fsck/FsckDirInode.h>
#include <common/net/message/NetMessage.h>
#include <common/toolkit/FsckTk.h>

class LinkToLostAndFoundMsg : public NetMessageSerdes<LinkToLostAndFoundMsg>
{
   public:
      LinkToLostAndFoundMsg(EntryInfo* lostAndFoundInfo, FsckDirInodeList* dirInodes) :
         BaseType(NETMSGTYPE_LinkToLostAndFound)
      {
         this->lostAndFoundInfoPtr = lostAndFoundInfo;
         this->dirInodes = dirInodes;
         this->fileInodes = NULL;
         this->entryType = FsckDirEntryType_DIRECTORY;
      }

      LinkToLostAndFoundMsg(EntryInfo* lostAndFoundInfo, FsckFileInodeList* fileInodes) :
         BaseType(NETMSGTYPE_LinkToLostAndFound)
      {
         this->lostAndFoundInfoPtr = lostAndFoundInfo;
         this->fileInodes = fileInodes;
         this->dirInodes = NULL;
         this->entryType = FsckDirEntryType_REGULARFILE;
      }

      LinkToLostAndFoundMsg() : BaseType(NETMSGTYPE_LinkToLostAndFound)
      {
      }

   private:
      FsckDirEntryType entryType; // to indicate, whether dir inodes or file inodes should be
                                  // linked; important for serialization

      // for serialization
      FsckDirInodeList* dirInodes; // not owned by this object
      FsckFileInodeList* fileInodes; // not owned by this object

      EntryInfo* lostAndFoundInfoPtr; // not owned by this object

      // for deserialization
      EntryInfo lostAndFoundInfo;

      // for deserialization
      struct {
         FsckDirInodeList dirInodes;
         FsckFileInodeList fileInodes;
      } parsed;

   public:
      FsckDirInodeList& getDirInodes()
      {
         return *dirInodes;
      }

      FsckFileInodeList& getFileInodes()
      {
         return *fileInodes;
      }

      EntryInfo* getLostAndFoundInfo()
      {
         return &(this->lostAndFoundInfo);
      }

      FsckDirEntryType getEntryType()
      {
         return this->entryType;
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::as<int32_t>(obj->entryType)
            % serdes::backedPtr(obj->lostAndFoundInfoPtr, obj->lostAndFoundInfo);

         if (FsckDirEntryType_ISDIR(obj->entryType))
            ctx % serdes::backedPtr(obj->dirInodes, obj->parsed.dirInodes);
         else
            ctx % serdes::backedPtr(obj->fileInodes, obj->parsed.fileInodes);
      }
};


#endif /*LINKTOLOSTANDFOUNDMSG_H*/
