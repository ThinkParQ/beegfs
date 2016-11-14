#ifndef CLOSECHUNKFILEMSG_H_
#define CLOSECHUNKFILEMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/nodes/NumNodeID.h>
#include <common/storage/PathInfo.h>


#define CLOSECHUNKFILEMSG_FLAG_NODYNAMICATTRIBS    1 /* skip retrieval of current dyn attribs */
#define CLOSECHUNKFILEMSG_FLAG_WRITEENTRYINFO      2 /* write entryinfo as chunk xattrs */
#define CLOSECHUNKFILEMSG_FLAG_BUDDYMIRROR         4 /* given targetID is a buddymirrorgroup ID */
#define CLOSECHUNKFILEMSG_FLAG_BUDDYMIRROR_SECOND  8 /* secondary of group, otherwise primary */


class CloseChunkFileMsg : public NetMessageSerdes<CloseChunkFileMsg>
{
   public:
      /**
       * @param fileHandleID just a reference, so do not free it as long as you use this object!
       * @param pathInfo just a reference, so do not free it as long as you use this object!
       */
      CloseChunkFileMsg(const NumNodeID sessionID, const std::string& fileHandleID,
         const uint16_t targetID, const PathInfo* pathInfo) :
         BaseType(NETMSGTYPE_CloseChunkFile)
      {
         this->sessionID = sessionID;

         this->fileHandleID = fileHandleID.c_str();
         this->fileHandleIDLen = fileHandleID.length();

         this->targetID = targetID;

         this->pathInfoPtr = pathInfo;

         // the EntryInfo, which will be attached in the chunk's xattrs; not set per default
         this->entryInfoBuf = NULL;
         this->entryInfoBufLen = 0;
      }

      /**
       * For deserialization only!
       */
      CloseChunkFileMsg() : BaseType(NETMSGTYPE_CloseChunkFile) { }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->sessionID
            % serdes::rawString(obj->fileHandleID, obj->fileHandleIDLen, 4)
            % serdes::backedPtr(obj->pathInfoPtr, obj->pathInfo);

         if(obj->isMsgHeaderFeatureFlagSet(CLOSECHUNKFILEMSG_FLAG_WRITEENTRYINFO) )
            ctx
               % obj->entryInfoBufLen
               % serdes::rawBlock(obj->entryInfoBuf, obj->entryInfoBufLen);

         ctx % obj->targetID;
      }

      unsigned getSupportedHeaderFeatureFlagsMask() const
      {
         return CLOSECHUNKFILEMSG_FLAG_NODYNAMICATTRIBS | CLOSECHUNKFILEMSG_FLAG_WRITEENTRYINFO |
            CLOSECHUNKFILEMSG_FLAG_BUDDYMIRROR | CLOSECHUNKFILEMSG_FLAG_BUDDYMIRROR_SECOND;
      }


   private:
      NumNodeID sessionID;
      unsigned fileHandleIDLen;
      const char* fileHandleID;
      uint16_t targetID;

      // for serialization
      const PathInfo *pathInfoPtr;

      // for deserialization
      PathInfo pathInfo;

      uint32_t entryInfoBufLen;
      const char* entryInfoBuf;

   public:
      // getters & setters

      NumNodeID getSessionID() const
      {
         return sessionID;
      }
      
      const char* getFileHandleID() const
      {
         return fileHandleID;
      }

      uint16_t getTargetID() const
      {
         return targetID;
      }

      const char* getEntryInfoBuf() const
      {
         return entryInfoBuf;
      }

      unsigned getEntryInfoBufLen() const
      {
         return entryInfoBufLen;
      }

      void setEntryInfoBuf(const char* entryInfoBuf, unsigned entryInfoBufLen)
      {
         this->entryInfoBuf = entryInfoBuf;
         this->entryInfoBufLen = entryInfoBufLen;

         this->addMsgHeaderFeatureFlag(CLOSECHUNKFILEMSG_FLAG_WRITEENTRYINFO);
      }

      const PathInfo* getPathInfo() const
      {
         return &this->pathInfo;
      }

      virtual TestingEqualsRes testingEquals(NetMessage* msg)
      {
         CloseChunkFileMsg* msgIn = (CloseChunkFileMsg*) msg;

         if (this->sessionID != msgIn->getSessionID())
            return TestingEqualsRes_FALSE;

         if ( strcmp (this->fileHandleID, msgIn->getFileHandleID()) != 0 )
            return TestingEqualsRes_FALSE;

         if ( this->targetID != msgIn->getTargetID() )
            return TestingEqualsRes_FALSE;

         unsigned featureFlags = this->getMsgHeaderFeatureFlags();
         if ( featureFlags  != msgIn->getMsgHeaderFeatureFlags() )
            return TestingEqualsRes_FALSE;


         if (featureFlags & CLOSECHUNKFILEMSG_FLAG_WRITEENTRYINFO)
         {
            if ( this->entryInfoBufLen != msgIn->getEntryInfoBufLen() )
               return TestingEqualsRes_FALSE;

            if ( (entryInfoBuf != NULL) && (msgIn->getEntryInfoBuf() != NULL) )
            {
               int memcmpRes = memcmp(this->entryInfoBuf, msgIn->getEntryInfoBuf(),
                  this->entryInfoBufLen);
               if ( memcmpRes  != 0 )
                  return TestingEqualsRes_FALSE;
            }
            else
            if ( ! (entryInfoBuf == NULL) && (msgIn->getEntryInfoBuf() == NULL) )
               return TestingEqualsRes_FALSE;
         }

         return TestingEqualsRes_TRUE;
      }
};


#endif /*CLOSECHUNKFILEMSG_H_*/
