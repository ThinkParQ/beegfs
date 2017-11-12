#ifndef RMDIRMSG_H_
#define RMDIRMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/Path.h>

class RmDirMsg : public NetMessage
{
   friend class AbstractNetMessageFactory;
   
   public:
      
      /**
       * @param parentInfo just a reference, so do not free it as long as you use this object!
       */
      RmDirMsg(EntryInfo* parentInfo, std::string& delDirName) : NetMessage(NETMSGTYPE_RmDir)
      {
         this->parentInfoPtr = parentInfo;
         this->delDirName    = delDirName;
      }


   protected:
      RmDirMsg() : NetMessage(NETMSGTYPE_RmDir)
      {
      }


      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);
      
      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH                                       +
            this->parentInfoPtr->serialLen()                               + // parentInfo
            Serialization::serialLenStrAlign4(this->delDirName.length() );   // delDirName
      }


   private:
      std::string delDirName;

      // for serialization
      EntryInfo* parentInfoPtr;


      // for deserialization
      EntryInfo parentInfo;


   public:
   
      // inliners

      // getters & setters
      EntryInfo* getParentInfo(void)
      {
         return &this->parentInfo;
      }

      std::string getDelDirName(void)
      {
         return this->delDirName;
      }

};


#endif /*RMDIRMSG_H_*/
