#ifndef MOVINGFILEINSERTMSG_H_
#define MOVINGFILEINSERTMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/Path.h>

/**
 * Used to tell a remote metadata server a file is to be moved between directories
 */
class MovingFileInsertMsg : public NetMessage
{
   friend class AbstractNetMessageFactory;
   
   public:
      
      /**
       * @param path just a reference, so do not free it as long as you use this object!
       * @param serialBuf serialized file;
       * just a reference, so do not free it as long as you use this object!
       */
      MovingFileInsertMsg(EntryInfo* fromFileInfo, EntryInfo* toDirInfo, std::string& newName,
         const char* serialBuf, unsigned serialBufLen) :  NetMessage(NETMSGTYPE_MovingFileInsert)
      {
         this->fromFileInfoPtr   = fromFileInfo;
         this->newName           = newName;
         this->toDirInfoPtr      = toDirInfo;

         this->serialBuf         = serialBuf;
         this->serialBufLen      = serialBufLen;
      }


   protected:
      MovingFileInsertMsg() : NetMessage(NETMSGTYPE_MovingFileInsert)
      {
      }


      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);
      
      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH                                   +
            this->fromFileInfoPtr->serialLen()                         +
            this->toDirInfoPtr->serialLen()                            + // toDirInfo
            Serialization::serialLenStrAlign4(this->newName.length() ) + // newName
            Serialization::serialLenUInt()                             + // serialBufLen
            serialBufLen;                                                // serial file buffer
      }


   private:
      EntryInfo  fromFileInfo; // for deserialization
      EntryInfo* fromFileInfoPtr; // for serialization

      std::string newName;

      const char* serialBuf;
      unsigned serialBufLen;

      EntryInfo* toDirInfoPtr; // for serialization
      EntryInfo  toDirInfo; // for deserialization

   public:

      // getters & setters
      const char* getSerialBuf()
      {
         return serialBuf;
      }
      
      EntryInfo* getToDirInfo()
      {
         return &this->toDirInfo;
      }

      std::string getNewName()
      {
         return this->newName;
      }

      EntryInfo* getFromFileInfo()
      {
         return &this->fromFileInfo;
      }
};

#endif /*MOVINGFILEINSERTMSG_H_*/
