#ifndef MOVEFILEINODEMSG_H_
#define MOVEFILEINODEMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>

class MoveFileInodeMsg : public MirroredMessageBase<MoveFileInodeMsg>
{
   public:
      /**
       * @param entryInfo just a reference, so do not free it as long as you use this object!
       * @param createLink If True then also increment link count after deinlining inode
       */
      MoveFileInodeMsg(EntryInfo* fromFileInfo, FileInodeMode mode, bool createLink = false) :
            BaseType(NETMSGTYPE_MoveFileInode), moveMode(mode), createHardlink(createLink),
            fromFileInfoPtr(fromFileInfo)
      {
      }

      MoveFileInodeMsg(EntryInfo* fromFileInfo) :
         BaseType(NETMSGTYPE_MoveFileInode), moveMode(MODE_INVALID), fromFileInfoPtr(fromFileInfo)
      {
      }

      /**
       * For deserialization only!
       */
      MoveFileInodeMsg() : BaseType(NETMSGTYPE_MoveFileInode) {}

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx % obj->moveMode;
         ctx % obj->createHardlink;
         ctx
            % serdes::backedPtr(obj->fromFileInfoPtr, obj->fromFileInfo);
      }

      bool supportsMirroring() const { return true; }

   private:
      int32_t moveMode;    // specify what kind of move operation is needed
      bool createHardlink; // specify whether new hardlink should be created or not

      // for serialization
      EntryInfo* fromFileInfoPtr;

      // for deserialization
      EntryInfo fromFileInfo;

   public:

      FileInodeMode getMode() const
      {
         return static_cast<FileInodeMode>(this->moveMode);
      }

      bool getCreateHardlink() const
      {
         return this->createHardlink;
      }

      EntryInfo* getFromFileEntryInfo()
      {
         return this->fromFileInfoPtr;
      }
};

#endif /* MOVEFILEINODEMSG_H_ */
