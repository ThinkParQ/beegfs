#ifndef CLOSEFILEMSG_H_
#define CLOSEFILEMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>
#include <common/storage/FileEvent.h>
#include <common/storage/StatData.h>
#include <common/storage/striping/DynamicFileAttribs.h>


#define CLOSEFILEMSG_FLAG_EARLYRESPONSE      1 /* send response before chunk files close */
#define CLOSEFILEMSG_FLAG_CANCELAPPENDLOCKS  2 /* cancel append locks of this file handle */
#define CLOSEFILEMSG_FLAG_DYNATTRIBS         4 // has dynattribs for secondary
#define CLOSEFILEMSG_FLAG_HAS_EVENT          8 /* contains file event logging information */

class CloseFileMsg : public MirroredMessageBase<CloseFileMsg>
{
   public:
      /**
       * @param clientNumID
       * @param fileHandleID
       * @param entryInfo just a reference, so do not free it as long as you use this object!
       */
      CloseFileMsg(const NumNodeID clientNumID, const std::string& fileHandleID,
         EntryInfo* entryInfo, const int maxUsedNodeIndex) :
         BaseType(NETMSGTYPE_CloseFile)
      {
         this->clientNumID = clientNumID;

         this->fileHandleID = fileHandleID;

         this->entryInfoPtr = entryInfo;

         this->maxUsedNodeIndex = maxUsedNodeIndex;
      }

      CloseFileMsg() : BaseType(NETMSGTYPE_CloseFile)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->clientNumID
            % serdes::stringAlign4(obj->fileHandleID)
            % serdes::backedPtr(obj->entryInfoPtr, obj->entryInfo)
            % obj->maxUsedNodeIndex;

         if (obj->isMsgHeaderFeatureFlagSet(CLOSEFILEMSG_FLAG_DYNATTRIBS))
            ctx % obj->dynAttribs;

         if (obj->hasFlag(NetMessageHeader::Flag_BuddyMirrorSecond))
            ctx % obj->inodeTimestamps;

         if (obj->isMsgHeaderFeatureFlagSet(CLOSEFILEMSG_FLAG_HAS_EVENT))
            ctx % obj->fileEvent;
      }

      unsigned getSupportedHeaderFeatureFlagsMask() const
      {
         return CLOSEFILEMSG_FLAG_EARLYRESPONSE | CLOSEFILEMSG_FLAG_CANCELAPPENDLOCKS |
            CLOSEFILEMSG_FLAG_DYNATTRIBS | CLOSEFILEMSG_FLAG_HAS_EVENT;
      }

      bool supportsMirroring() const { return true; }

   private:
      NumNodeID clientNumID;
      std::string fileHandleID;
      int32_t maxUsedNodeIndex;
      FileEvent fileEvent;

      // for serialization
      EntryInfo* entryInfoPtr; // not owned by this object

      // for deserialization
      EntryInfo entryInfo;

   protected:
      DynamicFileAttribsVec dynAttribs;
      MirroredTimestamps inodeTimestamps;

   public:
      NumNodeID getClientNumID() const
      {
         return clientNumID;
      }

      std::string getFileHandleID() const
      {
         return fileHandleID;
      }

      int getMaxUsedNodeIndex() const
      {
         return maxUsedNodeIndex;
      }

      EntryInfo* getEntryInfo()
      {
         return &this->entryInfo;
      }

      const FileEvent* getFileEvent() const
      {
         if (isMsgHeaderFeatureFlagSet(CLOSEFILEMSG_FLAG_HAS_EVENT))
            return &fileEvent;
         else
            return nullptr;
      }
};

#endif /*CLOSEFILEMSG_H_*/
