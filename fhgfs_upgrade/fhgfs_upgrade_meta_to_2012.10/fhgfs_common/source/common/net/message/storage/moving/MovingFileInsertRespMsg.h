#ifndef MOVINGFILEINSERTRESPMSG_H_
#define MOVINGFILEINSERTRESPMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/striping/StripePattern.h>
#include <common/Common.h>


class MovingFileInsertRespMsg : public NetMessage
{
   public:
      /**
       * @param unlinkedFileID set to empty string if no file was unlinked
       * @param pattern pattern of the unlinked file
       */
      MovingFileInsertRespMsg(int result, const char* unlinkedFileID, StripePattern* pattern,
         unsigned chunkHash) :
         NetMessage(NETMSGTYPE_MovingFileInsertResp)
      {
         this->result            = result;
         this->unlinkedFileID    = unlinkedFileID;
         this->unlinkedFileIDLen = strlen(unlinkedFileID);
         this->pattern           = pattern;
         this->chunkHash         = chunkHash;
      }

      MovingFileInsertRespMsg() : NetMessage(NETMSGTYPE_MovingFileInsertResp)
      {
      }
   
   protected:

      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);
      
      virtual unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenInt()                   + // result
            Serialization::serialLenUInt()                  + // chunkHash
            Serialization::serialLenStr(unlinkedFileIDLen)  +
            pattern->getSerialPatternLength();
      }
      
   private:
      int result;
      unsigned unlinkedFileIDLen;
      const char* unlinkedFileID;

      // for serialization
      StripePattern* pattern; // not owned by this object!
      
      // for deserialization
      StripePatternHeader patternHeader;
      const char* patternStart;
      
      unsigned chunkHash;

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
         return this->result;
      }
      
      const char* getUnlinkedFileID()
      {
         return this->unlinkedFileID;
      }
      
      unsigned getChunkHash()
      {
         return this->chunkHash;
      }

};

#endif /*MOVINGFILEINSERTRESPMSG_H_*/
