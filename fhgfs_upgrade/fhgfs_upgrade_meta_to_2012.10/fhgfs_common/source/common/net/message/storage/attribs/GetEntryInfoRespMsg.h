#ifndef GETENTRYINFORESPMSG_H_
#define GETENTRYINFORESPMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/striping/StripePattern.h>
#include <common/Common.h>


class GetEntryInfoRespMsg : public NetMessage
{
   public:
      GetEntryInfoRespMsg(int result, StripePattern* pattern) :
         NetMessage(NETMSGTYPE_GetEntryInfoResp)
      {
         this->result = result;
         this->pattern = pattern;
      }

      GetEntryInfoRespMsg() : NetMessage(NETMSGTYPE_GetEntryInfoResp)
      {
      }
   
   protected:

      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);
      
      virtual unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenInt() + // result
            pattern->getSerialPatternLength();
      }
      
   private:
      int result;

      // for serialization
      StripePattern* pattern; // not owned by this object!
      
      // for deserialization
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

      // getters & setters
      
      int getResult()
      {
         return result;
      }
      
};


#endif /*GETENTRYINFORESPMSG_H_*/
