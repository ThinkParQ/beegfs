#ifndef RMDIRMSG_H_
#define RMDIRMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>
#include <common/storage/FileEvent.h>
#include <common/storage/StatData.h>

#define RMDIRMSG_FLAG_HAS_EVENT       1 /* contains file event logging information */

class RmDirMsg : public MirroredMessageBase<RmDirMsg>
{
   friend class AbstractNetMessageFactory;

   public:

      /**
       * @param parentInfo just a reference, so do not free it as long as you use this object!
       */
      RmDirMsg(EntryInfo* parentInfo, std::string& delDirName) : BaseType(NETMSGTYPE_RmDir)
      {
         this->parentInfoPtr = parentInfo;
         this->delDirName    = delDirName;
      }

      RmDirMsg() : BaseType(NETMSGTYPE_RmDir)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::backedPtr(obj->parentInfoPtr, obj->parentInfo)
            % serdes::stringAlign4(obj->delDirName);

         if (obj->isMsgHeaderFeatureFlagSet(RMDIRMSG_FLAG_HAS_EVENT))
            ctx % obj->fileEvent;

         if (obj->hasFlag(NetMessageHeader::Flag_BuddyMirrorSecond))
            ctx % obj->dirTimestamps;
      }

      bool supportsMirroring() const { return true; }

   private:
      std::string delDirName;
      FileEvent fileEvent;

      // for serialization
      EntryInfo* parentInfoPtr;


      // for deserialization
      EntryInfo parentInfo;

   protected:
      MirroredTimestamps dirTimestamps;

   public:

      // inliners

      // getters & setters
      EntryInfo* getParentInfo(void)
      {
         return &this->parentInfo;
      }

      std::string getDelDirName(void)
      {
         return this->delDirName;
      }

      const FileEvent* getFileEvent() const
      {
         if (isMsgHeaderFeatureFlagSet(RMDIRMSG_FLAG_HAS_EVENT))
            return &fileEvent;
         else
            return nullptr;
      }

      unsigned getSupportedHeaderFeatureFlagsMask() const
      {
         return RMDIRMSG_FLAG_HAS_EVENT;
      }
};


#endif /*RMDIRMSG_H_*/
