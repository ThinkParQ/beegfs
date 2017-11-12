#ifndef SETDIRPATTERNMSG_H_
#define SETDIRPATTERNMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/striping/StripePattern.h>
#include <common/storage/EntryInfo.h>
#include <common/storage/Path.h>
#include <common/Common.h>


class SetDirPatternMsg : public NetMessage
{
   public:
      /**
       * @param path just a reference, so do not free it as long as you use this object!
       * @param pattern just a reference, so do not free it as long as you use this object!
       */
      SetDirPatternMsg(EntryInfo* entryInfo, StripePattern* pattern) :
         NetMessage(NETMSGTYPE_SetDirPattern)
      {
         this->entryInfoPtr = entryInfo;

         this->pattern = pattern;
      }

      SetDirPatternMsg() : NetMessage(NETMSGTYPE_SetDirPattern)
      {
      }
   
   protected:

      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);
      
      virtual unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            entryInfoPtr->serialLen() +
            pattern->getSerialPatternLength();
      }
      
   private:

      // for serialization
      EntryInfo* entryInfoPtr;

      StripePattern* pattern; // not owned by this object!

      // for deserialization
      EntryInfo entryInfo;
      StripePatternHeader patternHeader;
      const char* patternStart;
      
   public:
      // inliners

      /**
       * @return must be deleted by the caller; might be of type STRIPEPATTERN_Invalid
       */
      StripePattern* createPattern()
      {
         return StripePattern::createFromBuf(patternStart, patternHeader);
      }

      EntryInfo* getEntryInfo(void)
      {
         return &this->entryInfo;
      }

};


#endif /*SETDIRPATTERNMSG_H_*/
