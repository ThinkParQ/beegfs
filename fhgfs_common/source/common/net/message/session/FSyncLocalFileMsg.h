#ifndef FSYNCLOCALFILEMSG_H_
#define FSYNCLOCALFILEMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/nodes/NumNodeID.h>


#define FSYNCLOCALFILEMSG_FLAG_NO_SYNC          1 /* if a sync is not needed */
#define FSYNCLOCALFILEMSG_FLAG_SESSION_CHECK    2 /* if session check should be done */
#define FSYNCLOCALFILEMSG_FLAG_BUDDYMIRROR      4 /* given targetID is a buddymirrorgroup ID */
#define FSYNCLOCALFILEMSG_FLAG_BUDDYMIRROR_SECOND 8 /* secondary of group, otherwise primary */


class FSyncLocalFileMsg : public NetMessageSerdes<FSyncLocalFileMsg>
{
   public:

      /**
       * @param sessionID
       * @param fileHandleID just a reference, so do not free it as long as you use this object!
       */
      FSyncLocalFileMsg(const NumNodeID sessionID, const char* fileHandleID,
         const uint16_t targetID)
         : BaseType(NETMSGTYPE_FSyncLocalFile)
      {
         this->sessionID = sessionID;

         this->fileHandleID = fileHandleID;
         this->fileHandleIDLen = strlen(fileHandleID);

         this->targetID = targetID;
      }

      /**
       * For deserialization only!
       */
      FSyncLocalFileMsg() : BaseType(NETMSGTYPE_FSyncLocalFile){}

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->sessionID
            % serdes::rawString(obj->fileHandleID, obj->fileHandleIDLen, 4)
            % obj->targetID;
      }

      unsigned getSupportedHeaderFeatureFlagsMask() const
      {
         return FSYNCLOCALFILEMSG_FLAG_BUDDYMIRROR | FSYNCLOCALFILEMSG_FLAG_NO_SYNC |
            FSYNCLOCALFILEMSG_FLAG_SESSION_CHECK | FSYNCLOCALFILEMSG_FLAG_BUDDYMIRROR_SECOND;
      }


   private:
      NumNodeID sessionID;
      const char* fileHandleID;
      unsigned fileHandleIDLen;
      uint16_t targetID;


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
};


#endif /*FSYNCLOCALFILEMSG_H_*/
