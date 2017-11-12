#ifndef RETRIEVECHUNKSMSG_H
#define RETRIEVECHUNKSMSG_H

#include <common/net/message/NetMessage.h>

class RetrieveChunksMsg : public NetMessage
{
   public:
      RetrieveChunksMsg(uint16_t targetID, unsigned hashDirNum, unsigned maxOutChunks,
         int64_t lastOffset):
         NetMessage(NETMSGTYPE_RetrieveChunks)
      {
         this->targetID = targetID;
         this->hashDirNum = hashDirNum;
         this->maxOutChunks = maxOutChunks;
         this->lastOffset = lastOffset;
      }

      /**
      * For deserialization only
      */
      RetrieveChunksMsg() : NetMessage(NETMSGTYPE_RetrieveChunks)
      {
      }


      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);
      
      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenUShort() + // targetID
            Serialization::serialLenUInt() + // maxCount
            Serialization::serialLenInt64() + // currentDirLoc
            Serialization::serialLenInt64(); // lastEntryLoc
      }


   private:
      uint16_t targetID;
      unsigned hashDirNum;
      unsigned maxOutChunks;
      int64_t lastOffset;

   public:
      uint16_t getTargetID() const
      {
         return this->targetID;
      }

      unsigned getHashDirNum() const
      {
         return this->hashDirNum;
      }

      uint getMaxOutChunks() const
      {
         return this->maxOutChunks;
      }

      int64_t getLastOffset() const
      {
         return this->lastOffset;
      }

      virtual TestingEqualsRes testingEquals(NetMessage* msg)
      {
         if ( msg->getMsgType() != this->getMsgType() )
            return TestingEqualsRes_FALSE;

         RetrieveChunksMsg* retrieveChunksMsg = (RetrieveChunksMsg*) msg;

         if ( retrieveChunksMsg->getTargetID() != this->getTargetID() )
            return TestingEqualsRes_FALSE;

         if ( retrieveChunksMsg->getHashDirNum() != this->getHashDirNum() )
            return TestingEqualsRes_FALSE;

         if ( retrieveChunksMsg->getMaxOutChunks() != this->getMaxOutChunks() )
            return TestingEqualsRes_FALSE;

         if ( retrieveChunksMsg->getLastOffset() != this->getLastOffset() )
            return TestingEqualsRes_FALSE;

         return TestingEqualsRes_TRUE;
      }
};


#endif /*RETRIEVECHUNKSMSG_H*/
