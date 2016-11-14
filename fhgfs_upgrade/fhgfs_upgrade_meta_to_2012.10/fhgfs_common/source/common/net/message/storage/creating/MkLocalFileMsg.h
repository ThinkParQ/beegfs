#ifndef MKLOCALFILEMSG_H_
#define MKLOCALFILEMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/Path.h>
#include <common/storage/striping/StripePattern.h>

class MkLocalFileMsg : public NetMessage
{
   friend class AbstractNetMessageFactory;
   
   public:
      
      /**
       * @param path just a reference, so do not free it as long as you use this object!
       * @param pattern just a reference, so do not free it as long as you use this object!
       */
      MkLocalFileMsg(Path* path, StripePattern* pattern) : NetMessage(NETMSGTYPE_MkLocalFile)
      {
         this->path = path;
         this->pattern = pattern;
      }

     
   protected:
      MkLocalFileMsg() : NetMessage(NETMSGTYPE_MkLocalFile)
      {
      }


      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);
      
      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenPath(path) +
            pattern->getSerialPatternLength();
      }


   private:
      // for serialization
      Path* path; // not owned by this object!
      StripePattern* pattern; // not owned by this object!

      // for deserialization
      PathDeserializationInfo pathDeserInfo;
      StripePatternHeader patternHeader;
      const char* patternStart;


   public:
   
      // inliners   
      void parsePath(Path* outPath)
      {
         Serialization::deserializePath(pathDeserInfo, outPath);
      }
      
      /**
       * @return must be deleted by the caller; might be of type STRIPEPATTERN_Invalid
       */
      StripePattern* createPattern()
      {
         return StripePattern::createFromBuf(patternStart, patternHeader);
      }

      // getters & setters

};

#endif /*MKLOCALFILEMSG_H_*/
