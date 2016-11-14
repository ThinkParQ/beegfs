#ifndef CLOSEFILEMSG_H_
#define CLOSEFILEMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>

class CloseFileMsg : public NetMessage
{
   public:
      
      /**
       * @param sessionID just a reference, so do not free it as long as you use this object!
       * @param fileHandleID just a reference, so do not free it as long as you use this object!
       * @param entryInfo just a reference, so do not free it as long as you use this object!
       */
      CloseFileMsg(std::string& sessionID, std::string& fileHandleID, EntryInfo* entryInfo,
         int maxUsedNodeIndex) :  NetMessage(NETMSGTYPE_CloseFile)
      {
         this->sessionID = sessionID;
         
         this->fileHandleID = fileHandleID;
         
         this->entryInfoPtr = entryInfo;

         this->maxUsedNodeIndex = maxUsedNodeIndex;
      }

   protected:
      CloseFileMsg() : NetMessage(NETMSGTYPE_CloseFile)
      {
      }


      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);
      
      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenStrAlign4(this->sessionID.length() )    + // sessionID
            Serialization::serialLenStrAlign4(this->fileHandleID.length() ) + // fileHandleID
            this->entryInfoPtr->serialLen()                                 + // entryInfo
            Serialization::serialLenInt();                                    // maxUsedNodeIndex
      }


   private:
      std::string sessionID;
      std::string fileHandleID;
      int maxUsedNodeIndex;

      // for serialization
      EntryInfo* entryInfoPtr; // not owned by this object

      // for deserialization
      EntryInfo entryInfo;


   public:
   
      // getters & setters
      std::string getSessionID() const
      {
         return sessionID;
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

};

#endif /*CLOSEFILEMSG_H_*/
