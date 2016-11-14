#ifndef SETLOCALATTRMSG_H_
#define SETLOCALATTRMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/Path.h>
#include <common/storage/StorageDefinitions.h>


class SetLocalAttrMsg : public NetMessage
{
   friend class AbstractNetMessageFactory;
   
   public:
      
      /**
       * @param entryID just a reference, so do not free it as long as you use this object!
       * @param validAttribs a combination of SETATTR_CHANGE_...-Flags
       */
      SetLocalAttrMsg(const char* entryID, uint16_t targetID, int validAttribs,
         SettableFileAttribs* attribs, bool enableCreation) : NetMessage(NETMSGTYPE_SetLocalAttr)
      {
         this->entryID = entryID;
         this->entryIDLen = strlen(entryID);
         this->targetID = targetID;

         this->validAttribs = validAttribs;
         this->attribs = *attribs;
         this->enableCreation = enableCreation;
      }

      /**
       * @param entryID just a reference, so do not free it as long as you use this object!
       * @param targetID just a reference, so do not free it as long as you use this object!
       * @param validAttribs a combination of SETATTR_CHANGE_...-Flags
       */
      SetLocalAttrMsg(std::string& entryID, uint16_t targetID, int validAttribs,
         SettableFileAttribs* attribs, bool enableCreation) : NetMessage(NETMSGTYPE_SetLocalAttr)
      {
         this->entryID = entryID.c_str();
         this->entryIDLen = entryID.length();
         this->targetID = targetID;
         this->mirroredFromTargetID = 0;

         this->validAttribs = validAttribs;
         this->attribs = *attribs;
         this->enableCreation = enableCreation;
      }

   protected:
      /**
       * For deserialization only!
       */
      SetLocalAttrMsg() : NetMessage(NETMSGTYPE_SetLocalAttr) {}


      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);
      
      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenInt64() + // modificationTimeSecs
            Serialization::serialLenInt64() + // lastAccessTimeSecs
            Serialization::serialLenInt() + // validAttribs
            Serialization::serialLenInt() + // mode
            Serialization::serialLenUInt() + // userID
            Serialization::serialLenUInt() + // groupID
            Serialization::serialLenStrAlign4(entryIDLen) +
            Serialization::serialLenUShort() + // targetID
            Serialization::serialLenUShort() + // mirroredFromTargetID
            Serialization::serialLenBool(); // enableCreation
      }


   private:
      unsigned entryIDLen;
      const char* entryID;
      uint16_t targetID;
      uint16_t mirroredFromTargetID; // chunk file is a mirrored version from this targetID
      int validAttribs;
      SettableFileAttribs attribs;
      bool enableCreation;
   

   public:
   
      // getters & setters
      const char* getEntryID()
      {
         return entryID;
      }
      
      uint16_t getTargetID()
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

      int getValidAttribs()
      {
         return validAttribs;
      }
      
      SettableFileAttribs* getAttribs()
      {
         return &attribs;
      }
      
      bool getEnableCreation()
      {
         return enableCreation;
      }


};

#endif /*SETLOCALATTRMSG_H_*/
