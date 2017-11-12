#ifndef MKFILEWITHPATTERNMSG_H_
#define MKFILEWITHPATTERNMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/striping/StripePattern.h>
#include <common/storage/Path.h>

class MkFileWithPatternMsg : public NetMessage
{
   public:
      
      /**
       * @param path just a reference, so do not free it as long as you use this object!
       * @param pattern just a reference, so do not free it as long as you use this object!
       */
      MkFileWithPatternMsg(EntryInfo* parentInfo, std::string& newFileName,
         unsigned userID, unsigned groupID, int mode,
         StripePattern* pattern) : NetMessage(NETMSGTYPE_MkFileWithPattern)
      {
         this->parentInfoPtr = parentInfo;

         this->newFileName    = newFileName.c_str();
         this->newFileNameLen = newFileName.length();

         this->userID = userID;
         this->groupID = groupID;
         this->mode = mode;
         this->pattern = pattern;
      }


      /**
       * Constructor for deserialization only
       */
      MkFileWithPatternMsg() : NetMessage(NETMSGTYPE_MkFileWithPattern)
      {
      }

   protected:
      
      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);
      
      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenUInt() + // userID
            Serialization::serialLenUInt() + // groupID
            Serialization::serialLenInt() + // mode
            this->parentInfoPtr->serialLen() + // parentInfoP
            Serialization::serialLenStrAlign4(this->newFileNameLen) + // newFileName
            pattern->getSerialPatternLength();
      }


   private:
      
      const char* newFileName;
      unsigned newFileNameLen;

      unsigned userID;
      unsigned groupID;
      int mode;
   
      // for serialization
      EntryInfo* parentInfoPtr; // not owned by this object!
      StripePattern* pattern; // not owned by this object!

      // for deserialization
      EntryInfo parentInfo;
      StripePatternHeader patternHeader;
      const char* patternStart;

   public:
   

      /**
       * @return must be deleted by the caller; might be of type STRIPEPATTERN_Invalid
       */
      StripePattern* createPattern()
      {
         return StripePattern::createFromBuf(patternStart, patternHeader);
      }
      
      // getters & setters
      unsigned getUserID()
      {
         return userID;
      }
      
      unsigned getGroupID()
      {
         return groupID;
      }
      
      int getMode()
      {
         return mode;
      }

      EntryInfo* getParentInfo(void)
      {
         return &this->parentInfo;
      }

      const char* getNewFileName(void)
      {
         return this->newFileName;
      }

};

#endif /* MKFILEWITHPATTERNMSG_H_ */
