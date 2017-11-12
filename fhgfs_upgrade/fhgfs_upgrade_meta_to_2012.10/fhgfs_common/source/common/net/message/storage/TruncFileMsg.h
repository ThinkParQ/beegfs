#ifndef TRUNCFILEMSG_H_
#define TRUNCFILEMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/Path.h>


class TruncFileMsg : public NetMessage
{
   public:

      /**
       * @param entryID just a reference, so do not free it as long as you use this object!
       */
      TruncFileMsg(int64_t filesize, EntryInfo* entryInfo) :
         NetMessage(NETMSGTYPE_TruncFile)
      {
         this->filesize = filesize;

         this->entryInfoPtr = entryInfo;
      }
     
   protected:
      /**
       * For deserialization only!
       */
      TruncFileMsg() : NetMessage(NETMSGTYPE_TruncFile) { }


      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);
      
      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenInt64() + // filesize
            this->entryInfoPtr->serialLen();  // entryInfo
      }


   private:
      int64_t filesize;

      // for serialization
      EntryInfo* entryInfoPtr; // not owned by this object

      // for deserialization
      EntryInfo entryInfo;


   public:
   
      // getters & setters
      int64_t getFilesize()
      {
         return filesize;
      }
      
      EntryInfo* getEntryInfo()
      {
         return &this->entryInfo;
      }
      

};

#endif /*TRUNCFILEMSG_H_*/
