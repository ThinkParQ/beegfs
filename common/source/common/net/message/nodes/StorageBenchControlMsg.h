#ifndef STORAGEBENCHCONTROLMSG_H_
#define STORAGEBENCHCONTROLMSG_H_

#include <common/Common.h>
#include <common/benchmark/StorageBench.h>
#include <common/net/message/NetMessage.h>
#include <common/toolkit/serialization/Serialization.h>


class StorageBenchControlMsg: public NetMessageSerdes<StorageBenchControlMsg>
{
   public:
      StorageBenchControlMsg(StorageBenchAction action, StorageBenchType type, int64_t blocksize,
         int64_t size, int threads, bool odirect, UInt16List* targetIDs)
      : BaseType(NETMSGTYPE_StorageBenchControlMsg)
      {
         this->action = action;
         this->type = type;
         this->blocksize = blocksize;
         this->size = size;
         this->threads = threads;
         this->odirect = odirect;
         this->targetIDs = targetIDs;
      }

      /**
       * Constructor for deserialization only!
       */
      StorageBenchControlMsg() : BaseType(NETMSGTYPE_StorageBenchControlMsg)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->action
            % obj->type
            % obj->blocksize
            % obj->size
            % obj->threads
            % obj->odirect
            % serdes::backedPtr(obj->targetIDs, obj->parsed.targetIDs);
      }

   private:
      int32_t action;             // StorageBenchAction
      int32_t type;               // StorageBenchType
      int64_t blocksize;
      int64_t size;
      int32_t threads;
      bool odirect;
      UInt16List* targetIDs;

      // deserialization info
      struct {
         UInt16List targetIDs;
      } parsed;

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

      bool getODirect()
      {
         return this->odirect;
      }

      UInt16List& getTargetIDs()
      {
         return *targetIDs;
      }
};

#endif /* STORAGEBENCHCONTROLMSG_H_ */
