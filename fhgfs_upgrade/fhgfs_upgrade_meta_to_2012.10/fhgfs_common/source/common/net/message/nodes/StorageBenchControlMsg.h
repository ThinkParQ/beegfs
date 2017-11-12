#ifndef STORAGEBENCHCONTROLMSG_H_
#define STORAGEBENCHCONTROLMSG_H_

#include <common/Common.h>
#include <common/benchmark/StorageBench.h>
#include <common/net/message/NetMessage.h>
#include <common/toolkit/serialization/Serialization.h>


class StorageBenchControlMsg: public NetMessage
{
   public:
      StorageBenchControlMsg(StorageBenchAction action, StorageBenchType type, int64_t blockzize,
         int64_t size, int threads, UInt16List* targetIDs)
      : NetMessage(NETMSGTYPE_StorageBenchControlMsg)
      {
         this->action = action;
         this->type = type;
         this->blocksize = blockzize;
         this->size = size;
         this->threads = threads;
         this->targetIDs = targetIDs;
      }

      /**
       * Constructor for deserialization only!
       */
      StorageBenchControlMsg() : NetMessage(NETMSGTYPE_StorageBenchControlMsg)
      {
      }

   protected:
      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);

      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenInt() + // action
            Serialization::serialLenInt() + // type
            Serialization::serialLenInt64() + // blocksize
            Serialization::serialLenInt64() + // size
            Serialization::serialLenInt() + // threads
            Serialization::serialLenUInt16List(this->targetIDs); // targetIDs
      }

   private:
      int action;             // StorageBenchAction
      int type;               // StorageBenchType
      int64_t blocksize;
      int64_t size;
      int threads;
      UInt16List* targetIDs;

      // deserialization info
      unsigned targetIDsElemNum;
      const char* targetIDListStart;
      unsigned targetIDsBufLen;

   public:
      //inliners

      StorageBenchAction getAction()
      {
         return (StorageBenchAction)this->action;
      }

      StorageBenchType getType()
      {
         return (StorageBenchType)this->type;
      }

      int64_t getBlocksize()
      {
         return this->blocksize;
      }

      int64_t getSize()
      {
         return this->size;
      }

      int getThreads()
      {
         return this->threads;
      }

      void parseTargetIDs(UInt16List* outTargetIds)
      {
         Serialization::deserializeUInt16List(this->targetIDsBufLen, this->targetIDsElemNum,
            this->targetIDListStart, outTargetIds);
      }
};

#endif /* STORAGEBENCHCONTROLMSG_H_ */
