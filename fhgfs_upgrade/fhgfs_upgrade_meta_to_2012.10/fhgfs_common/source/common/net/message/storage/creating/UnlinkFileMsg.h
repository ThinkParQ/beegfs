#ifndef UNLINKFILEMSG_H_
#define UNLINKFILEMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/Path.h>

class UnlinkFileMsg : public NetMessage
{
   friend class AbstractNetMessageFactory;
   
   public:
      
      /**
       * @param parentInfo just a reference, so do not free it as long as you use this object!
       */
      UnlinkFileMsg(EntryInfo* parentInfo, std::string& delFileName) :
         NetMessage(NETMSGTYPE_UnlinkFile)
      {
         this->parentInfoPtr  = parentInfo;
         this->delFileName = delFileName;
      }


   protected:
      UnlinkFileMsg() : NetMessage(NETMSGTYPE_UnlinkFile)
      {
      }


      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);
      
      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH                                        +
            this->parentInfoPtr->serialLen()                                + // parentInfo
            Serialization::serialLenStrAlign4(this->delFileName.length() );   // delFileName
      }


   private:
      std::string delFileName;

      // for serialization
      EntryInfo* parentInfoPtr; // not owned by this object!

      // for deserialization
      EntryInfo parentInfo;


   public:
   
      // inliners   

      // getters & setters
      EntryInfo* getParentInfo(void)
      {
         return &this->parentInfo;
      }

      std::string getDelFileName(void)
      {
         return this->delFileName;
      }

};


#endif /*UNLINKFILEMSG_H_*/
