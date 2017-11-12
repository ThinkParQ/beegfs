#ifndef MOVINGDIRINSERTMSG_H_
#define MOVINGDIRINSERTMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/Path.h>

class MovingDirInsertMsg : public NetMessage
{
   friend class AbstractNetMessageFactory;
   
   public:
      
      /**
       * @param path just a reference, so do not free it as long as you use this object!
       * @param serialBuf serialized dir-link;
       * just a reference, so do not free it as long as you use this object!
       */
      MovingDirInsertMsg(EntryInfo* toDirInfo, std::string& newName,
         const char* serialBuf, unsigned serialBufLen) :
         NetMessage(NETMSGTYPE_MovingDirInsert)
      {
         this->toDirInfoPtr = toDirInfo;
         this->newName      = newName;
         this->serialBuf    = serialBuf;
         this->serialBufLen = serialBufLen;
      }


   protected:
      MovingDirInsertMsg() : NetMessage(NETMSGTYPE_MovingDirInsert)
      {
      }


      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);
      
      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH                                   +
            this->toDirInfoPtr->serialLen()                            + // entryInfo
            Serialization::serialLenStrAlign4(this->newName.length() ) + // newName
            Serialization::serialLenUInt()                             + // serialBufLen
            serialBufLen; // serial file buffer
      }


   private:
      const char* serialBuf;
      unsigned serialBufLen;
      
      std::string newName;

      // for serialization
      EntryInfo* toDirInfoPtr;

      // for deserialization
      EntryInfo toDirInfo;

   public:
   
      EntryInfo* getToDirInfo(void)
      {
         return &this->toDirInfo;
      }

      std::string getNewName(void)
      {
         return this->newName;
      }

      const char* getSerialBuf(void)
      {
         return this->serialBuf;
      }
};

#endif /*MOVINGDIRINSERTMSG_H_*/
