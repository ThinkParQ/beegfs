#ifndef UNLINKLOCALFILEMSG_H_
#define UNLINKLOCALFILEMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/Path.h>


class UnlinkLocalFileMsg : public NetMessage
{
   friend class AbstractNetMessageFactory;
   
   public:
      
      /**
       * @param entryID just a reference, so do not free it as long as you use this object!
       */
      UnlinkLocalFileMsg(const char* entryID, uint16_t targetID) :
         NetMessage(NETMSGTYPE_UnlinkLocalFile)
      {
         this->entryID = entryID;
         this->entryIDLen = strlen(entryID);

         this->targetID = targetID;
         this->mirroredFromTargetID = 0;
      }

      /**
       * @param entryID just a reference, so do not free it as long as you use this object!
       */
      UnlinkLocalFileMsg(std::string& entryID, uint16_t targetID) :
         NetMessage(NETMSGTYPE_UnlinkLocalFile)
      {
         this->entryID = entryID.c_str();
         this->entryIDLen = entryID.length();

         this->targetID = targetID;
         this->mirroredFromTargetID = 0;
      }


   protected:
      /**
       * For deserialization only!
       */
      UnlinkLocalFileMsg() : NetMessage(NETMSGTYPE_UnlinkLocalFile) { }


      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);
      
      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenUShort() + // targetID
            Serialization::serialLenUShort() + // mirroredFromTargetID
            Serialization::serialLenStrAlign4(entryIDLen);
      }


   private:
      unsigned entryIDLen;
      const char* entryID;
      uint16_t targetID;
      uint16_t mirroredFromTargetID; // this chunk file is mirrored from this original target


   public:
   
      // getters & setters

      const char* getEntryID() const
      {
         return entryID;
      }

      uint16_t getTargetID() const
      {
         return targetID;
      }
      
      uint16_t getMirroredFromTargetID() const
      {
         return mirroredFromTargetID;
      }

      void setMirroredFromTargetID(uint16_t mirroredFromTargetID)
      {
         this->mirroredFromTargetID = mirroredFromTargetID;
      }

};


#endif /*UNLINKLOCALFILEMSG_H_*/
