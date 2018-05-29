#ifndef HARDLINKMSG_H_
#define HARDLINKMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>
#include <common/storage/FileEvent.h>
#include <common/storage/StatData.h>

#define HARDLINKMSG_FLAG_IS_TO_DENTRY_CREATE 1   /* NOTE: this flag isn't used anymore, but we keep
                                                  * it, to prevent us from re-using the number */
#define HARDLINKMSG_FLAG_HAS_EVENT           2 /* contains file event logging information */

class HardlinkMsg : public MirroredMessageBase<HardlinkMsg>
{
   friend class AbstractNetMessageFactory;

   public:

      /**
       * Use this NetMsg from a client to create a hard link. NetMsg send to fromDir meta node.
       * @param fromDirInfo the directory, where the initial file/link exists; just a reference, so
       * do not free it as long as you use this object!
       * @param fromName the initial inode data; just a reference, so do not free it as long as you
       * use this object!
       * @param fromInfo just a reference, so do not free it as long as you use this object!
       * @param toDirInfo the directory, where the new link will be creted; just a reference, so do
       * not free it as long as you use this object!
       * @param toName the name of the new link
       */
      HardlinkMsg(EntryInfo* fromDirInfo, std::string& fromName, EntryInfo* fromInfo,
          EntryInfo* toDirInfo, std::string& toName) : BaseType(NETMSGTYPE_Hardlink)
      {
         this->fromName        = fromName;
         this->fromInfoPtr     = fromInfo;

         this->fromDirInfoPtr  = fromDirInfo;

         this->toName          = toName;

         this->toDirInfoPtr    = toDirInfo;
      }

      /*
       * For deserialization
       */
      HardlinkMsg() : BaseType(NETMSGTYPE_Hardlink)
      {
      }

      static void serialize(const HardlinkMsg* obj, Serializer& ctx)
      {
         ctx
            % serdes::backedPtr(obj->fromInfoPtr, obj->fromInfo)
            % serdes::backedPtr(obj->toDirInfoPtr, obj->toDirInfo)
            % serdes::stringAlign4(obj->toName);

         if (obj->fromDirInfoPtr)
         {
            ctx
               % serdes::backedPtr(obj->fromDirInfoPtr, obj->fromDirInfo)
               % serdes::stringAlign4(obj->fromName);
         }

         if (obj->isMsgHeaderFeatureFlagSet(HARDLINKMSG_FLAG_HAS_EVENT))
            ctx % obj->fileEvent;

         if (obj->hasFlag(NetMessageHeader::Flag_BuddyMirrorSecond))
            ctx
               % obj->dirTimestamps
               % obj->fileTimestamps;
      }

      static void serialize(HardlinkMsg* obj, Deserializer& ctx)
      {
         ctx
            % serdes::backedPtr(obj->fromInfoPtr, obj->fromInfo)
            % serdes::backedPtr(obj->toDirInfoPtr, obj->toDirInfo)
            % serdes::stringAlign4(obj->toName)
            % serdes::backedPtr(obj->fromDirInfoPtr, obj->fromDirInfo)
            % serdes::stringAlign4(obj->fromName);

         if (obj->isMsgHeaderFeatureFlagSet(HARDLINKMSG_FLAG_HAS_EVENT))
            ctx % obj->fileEvent;

         if (obj->hasFlag(NetMessageHeader::Flag_BuddyMirrorSecond))
            ctx
               % obj->dirTimestamps
               % obj->fileTimestamps;
      }

   private:

      std::string fromName;
      std::string toName;
      FileEvent fileEvent;

      // for serialization
      EntryInfo* fromInfoPtr;    // not owned by this object
      EntryInfo* fromDirInfoPtr; // not owned by this object
      EntryInfo* toDirInfoPtr;   // not owned by this object

      // for deserialization
      EntryInfo fromInfo;
      EntryInfo fromDirInfo;
      EntryInfo toDirInfo;

   protected:
      MirroredTimestamps dirTimestamps;
      MirroredTimestamps fileTimestamps;

   public:
      // getters & setters

      EntryInfo* getFromInfo()
      {
         return &this->fromInfo;
      }

      EntryInfo* getFromDirInfo()
      {
         return &this->fromDirInfo;
      }

      EntryInfo* getToDirInfo()
      {
         return &this->toDirInfo;
      }

      std::string getFromName() const
      {
         return this->fromName;
      }

      std::string getToName() const
      {
         return this->toName;
      }

      bool supportsMirroring() const { return true; }

      const FileEvent* getFileEvent() const
      {
         if (isMsgHeaderFeatureFlagSet(HARDLINKMSG_FLAG_HAS_EVENT))
            return &fileEvent;
         else
            return nullptr;
      }

      unsigned getSupportedHeaderFeatureFlagsMask() const
      {
         return HARDLINKMSG_FLAG_HAS_EVENT;
      }
};


#endif /*HARDLINKMSG_H_*/
