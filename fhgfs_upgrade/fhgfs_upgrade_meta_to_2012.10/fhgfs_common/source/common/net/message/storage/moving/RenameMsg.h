#ifndef RENAMEMSG_H_
#define RENAMEMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/Path.h>

class RenameMsg : public NetMessage
{
   friend class AbstractNetMessageFactory;
   
   public:
      
      /**
       * @param fromDirInfo just a reference, so do not free it as long as you use this object!
       * @param toDirInfo just a reference, so do not free it as long as you use this object!
       */
      RenameMsg(std::string& oldName, EntryInfo* fromDirInfo, std::string& newName,
          DirEntryType entryType, EntryInfo* toDirInfo) : NetMessage(NETMSGTYPE_Rename)
      {
         this->oldName        = oldName;
         this->entryType      = entryType;
         this->fromDirInfoPtr = fromDirInfo;

         this->newName        = newName;
         this->toDirInfoPtr   = toDirInfo;
      }


   protected:
      RenameMsg() : NetMessage(NETMSGTYPE_Rename)
      {
      }


      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);
      
      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH                                       +
            Serialization::serialLenUInt()                                 + // entryType
            this->fromDirInfoPtr->serialLen()                              + // fromDirInfo
            this->toDirInfoPtr->serialLen()                                + // toDirInfo
            Serialization::serialLenStrAlign4(this->oldName.length() )     + // oldName
            Serialization::serialLenStrAlign4(this->newName.length() );      // newName
      }


   private:
      std::string oldName;
      std::string newName;

      DirEntryType entryType;

      // for serialization
      EntryInfo* fromDirInfoPtr;
      EntryInfo* toDirInfoPtr;

      // for deserialization
      EntryInfo fromDirInfo;
      EntryInfo toDirInfo;

   public:
   
      // inliners
      
      // getters & setters

      EntryInfo* getFromDirInfo(void)
      {
         return &this->fromDirInfo;
      }

      EntryInfo* getToDirInfo(void)
      {
         return &this->toDirInfo;
      }

      std::string getOldName(void)
      {
         return this->oldName;
      }

      std::string getNewName(void)
      {
         return this->newName;
      }

      DirEntryType getEntryType(void)
      {
         return this->entryType;
      }
};


#endif /*RENAMEMSG_H_*/
