#ifndef RENAMEMSG_H_
#define RENAMEMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>
#include <common/storage/FileEvent.h>
#include <common/storage/StatData.h>

#define RENAMEMSG_FLAG_HAS_EVENT       1 /* contains file event logging information */

class RenameMsg : public MirroredMessageBase<RenameMsg>
{
   friend class AbstractNetMessageFactory;

   public:

      /**
       * @param fromDirInfo just a reference, so do not free it as long as you use this object!
       * @param toDirInfo just a reference, so do not free it as long as you use this object!
       */
      RenameMsg(std::string& oldName, EntryInfo* fromDirInfo, std::string& newName,
          DirEntryType entryType, EntryInfo* toDirInfo) : BaseType(NETMSGTYPE_Rename)
      {
         this->oldName        = oldName;
         this->entryType      = entryType;
         this->fromDirInfoPtr = fromDirInfo;

         this->newName        = newName;
         this->toDirInfoPtr   = toDirInfo;
      }

      RenameMsg() : BaseType(NETMSGTYPE_Rename)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::as<uint32_t>(obj->entryType)
            % serdes::backedPtr(obj->fromDirInfoPtr, obj->fromDirInfo)
            % serdes::backedPtr(obj->toDirInfoPtr, obj->toDirInfo)
            % serdes::stringAlign4(obj->oldName)
            % serdes::stringAlign4(obj->newName);

         if (obj->isMsgHeaderFeatureFlagSet(RENAMEMSG_FLAG_HAS_EVENT))
            ctx % obj->fileEvent;

         if (obj->hasFlag(NetMessageHeader::Flag_BuddyMirrorSecond))
            ctx
               % obj->fromDirTimestamps
               % obj->renamedInodeTimestamps;
      }

      bool supportsMirroring() const { return true; }

   private:
      std::string oldName;
      std::string newName;

      DirEntryType entryType;
      FileEvent fileEvent;

      // for serialization
      EntryInfo* fromDirInfoPtr;
      EntryInfo* toDirInfoPtr;

      // for deserialization
      EntryInfo fromDirInfo;
      EntryInfo toDirInfo;

   protected:
      MirroredTimestamps fromDirTimestamps;
      MirroredTimestamps renamedInodeTimestamps;

   public:

      // inliners

      // getters & setters

      EntryInfo* getFromDirInfo(void)
      {
         return &this->fromDirInfo;
      }

      EntryInfo* getToDirInfo(void)
      {
         return &this->toDirInfo;
      }

      std::string getOldName(void)
      {
         return this->oldName;
      }

      std::string getNewName(void)
      {
         return this->newName;
      }

      DirEntryType getEntryType(void)
      {
         return this->entryType;
      }

      const FileEvent* getFileEvent() const
      {
         if (isMsgHeaderFeatureFlagSet(RENAMEMSG_FLAG_HAS_EVENT))
            return &fileEvent;
         else
            return nullptr;
      }

      unsigned getSupportedHeaderFeatureFlagsMask() const
      {
         return RENAMEMSG_FLAG_HAS_EVENT; }
};


#endif /*RENAMEMSG_H_*/
