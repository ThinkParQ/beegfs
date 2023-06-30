#ifndef MOVEFILEINODEMSG_H_
#define MOVEFILEINODEMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>

class MoveFileInodeMsg : public MirroredMessageBase<MoveFileInodeMsg>
{
   public:
      /**
       * @param entryInfo just a reference, so do not free it as long as you use this object!
       */
      MoveFileInodeMsg(EntryInfo* fromFileInfo, FileInodeMode mode) :
            BaseType(NETMSGTYPE_MoveFileInode), moveMode(mode), fromFileInfoPtr(fromFileInfo)
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
         ctx
            % serdes::backedPtr(obj->fromFileInfoPtr, obj->fromFileInfo);
      }

      bool supportsMirroring() const { return true; }

   private:
      int32_t moveMode; // specify what kind of operation needed

      // for serialization
      EntryInfo* fromFileInfoPtr;

      // for deserialization
      EntryInfo fromFileInfo;

   public:

      FileInodeMode getMode() const
      {
         return static_cast<FileInodeMode>(this->moveMode);
      }

      EntryInfo* getFromFileEntryInfo()
      {
         return this->fromFileInfoPtr;
      }
};

#endif /* MOVEFILEINODEMSG_H_ */
