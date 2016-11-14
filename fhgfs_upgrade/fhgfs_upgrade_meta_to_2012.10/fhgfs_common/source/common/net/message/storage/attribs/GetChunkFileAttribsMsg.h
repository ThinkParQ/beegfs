#ifndef GETCHUNKFILEATTRIBSMSG_H_
#define GETCHUNKFILEATTRIBSMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/Path.h>

class GetChunkFileAttribsMsg : public NetMessage
{
   friend class AbstractNetMessageFactory;
   
   public:
      
      /**
       * @param path just a reference, so do not free it as long as you use this object!
       */
      GetChunkFileAttribsMsg(const char* entryID, uint16_t targetID) :
         NetMessage(NETMSGTYPE_GetChunkFileAttribs)
      {
         this->entryID = entryID;
         this->entryIDLen = strlen(entryID);

         this->targetID = targetID;
      }

      GetChunkFileAttribsMsg(std::string& entryID, uint16_t targetID) :
         NetMessage(NETMSGTYPE_GetChunkFileAttribs)
      {
         this->entryID = entryID.c_str();
         this->entryIDLen = entryID.length();

         this->targetID = targetID;
      }


   protected:
      /**
       * For deserialization only
       */
      GetChunkFileAttribsMsg() : NetMessage(NETMSGTYPE_GetChunkFileAttribs)
      {
      }


      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);
      
      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenStrAlign4(entryIDLen) +
            Serialization::serialLenUShort(); // targetID
      }


   private:
      unsigned entryIDLen;
      const char* entryID;
      uint16_t targetID;


   public:
   
      // inliners   
      const char* getEntryID()
      {
         return entryID;
      }
      
      // getters & setters
      uint16_t getTargetID()
      {
         return targetID;
      }


};

#endif /*GETCHUNKFILEATTRIBSMSG_H_*/
