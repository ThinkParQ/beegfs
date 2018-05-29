#ifndef UNLINKFILEMSG_H_
#define UNLINKFILEMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>
#include <common/storage/FileEvent.h>
#include <common/storage/Path.h>
#include <common/storage/StatData.h>

#define UNLINKFILEMSG_FLAG_HAS_EVENT       1 /* contains file event logging information */

class UnlinkFileMsg : public MirroredMessageBase<UnlinkFileMsg>
{
   friend class AbstractNetMessageFactory;

   public:

      /**
       * @param parentInfo just a reference, so do not free it as long as you use this object!
       */
      UnlinkFileMsg(EntryInfo* parentInfo, std::string& delFileName) :
         BaseType(NETMSGTYPE_UnlinkFile)
      {
         this->parentInfoPtr  = parentInfo;
         this->delFileName = delFileName;
      }

      UnlinkFileMsg() : BaseType(NETMSGTYPE_UnlinkFile)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::backedPtr(obj->parentInfoPtr, obj->parentInfo)
            % serdes::stringAlign4(obj->delFileName);

         if (obj->isMsgHeaderFeatureFlagSet(UNLINKFILEMSG_FLAG_HAS_EVENT))
            ctx % obj->fileEvent;

         if (obj->hasFlag(NetMessageHeader::Flag_BuddyMirrorSecond))
            ctx
               % obj->dirTimestamps
               % obj->fileInfo
               % obj->fileTimestamps;
      }

      bool supportsMirroring() const { return true; }

   private:
      std::string delFileName;
      FileEvent fileEvent;

      // for serialization
      EntryInfo* parentInfoPtr; // not owned by this object!

      // for deserialization
      EntryInfo parentInfo;

   protected:
      MirroredTimestamps dirTimestamps;
      EntryInfo fileInfo;
      MirroredTimestamps fileTimestamps;

   public:
      // inliners

      // getters & setters
      EntryInfo* getParentInfo()
      {
         return &this->parentInfo;
      }

      std::string getDelFileName() const
      {
         return this->delFileName;
      }

      const FileEvent* getFileEvent() const
      {
         if (isMsgHeaderFeatureFlagSet(UNLINKFILEMSG_FLAG_HAS_EVENT))
            return &fileEvent;
         else
            return nullptr;
      }

      unsigned getSupportedHeaderFeatureFlagsMask() const
      {
         return UNLINKFILEMSG_FLAG_HAS_EVENT;
      }
};


#endif /*UNLINKFILEMSG_H_*/
