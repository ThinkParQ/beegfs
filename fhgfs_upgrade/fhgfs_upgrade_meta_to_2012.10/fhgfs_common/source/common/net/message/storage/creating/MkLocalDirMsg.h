#ifndef MKLOCALDIRMSG_H_
#define MKLOCALDIRMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/striping/StripePattern.h>
#include <common/storage/Path.h>

class MkLocalDirMsg : public NetMessage
{
   friend class AbstractNetMessageFactory;
   
   public:
      
      /**
       * @param entryInfo just a reference, so do not free it as long as you use this object!
       */
      MkLocalDirMsg(EntryInfo* entryInfo, unsigned userID, unsigned groupID, int mode,
         StripePattern* pattern, uint16_t parentNodeID) : NetMessage(NETMSGTYPE_MkLocalDir)
      {
         this->entryInfoPtr = entryInfo;
         this->userID = userID;
         this->groupID = groupID;
         this->mode = mode;
         this->pattern = pattern;
         this->parentNodeID = parentNodeID;
      }


   protected:
      /**
       * For deserialization only!
       */
      MkLocalDirMsg() : NetMessage(NETMSGTYPE_MkLocalDir) {}


      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);
      
      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenUInt()                                  + // userID
            Serialization::serialLenUInt()                                  + // groupID
            Serialization::serialLenInt()                                   + // mode
            entryInfoPtr->serialLen()                                       + // entryInfo
            pattern->getSerialPatternLength()                               +
            Serialization::serialLenUShort();                                 // parentNodeID
      }


   private:
      unsigned userID;
      unsigned groupID;
      int mode;
      uint16_t parentNodeID;
   
      // for serialization
      EntryInfo* entryInfoPtr; // not owned by this object!
      StripePattern* pattern;  // not owned by this object!

      // for deserialization
      EntryInfo entryInfo;
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
      unsigned getUserID() const
      {
         return userID;
      }
      
      unsigned getGroupID() const
      {
         return groupID;
      }
      
      int getMode() const
      {
         return mode;
      }
      
      uint16_t getParentNodeID() const
      {
         return this->parentNodeID;
      }

      EntryInfo* getEntryInfo(void)
      {
         return &this->entryInfo;
      }

};

#endif /*MKLOCALDIRMSG_H_*/
