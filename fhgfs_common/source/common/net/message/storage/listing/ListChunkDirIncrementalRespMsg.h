#ifndef LISTCHUNKDIRINCREMENTALRESPMSG_H_
#define LISTCHUNKDIRINCREMENTALRESPMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/Common.h>

class ListChunkDirIncrementalRespMsg : public NetMessageSerdes<ListChunkDirIncrementalRespMsg>
{
   public:
      ListChunkDirIncrementalRespMsg(FhgfsOpsErr result, StringList* names, IntList* entryTypes,
         int64_t newOffset) : BaseType(NETMSGTYPE_ListChunkDirIncrementalResp)
      {
         this->result = (int)result;
         this->names = names;
         this->entryTypes = entryTypes;
         this->newOffset = newOffset;
      }

      ListChunkDirIncrementalRespMsg() : BaseType(NETMSGTYPE_ListChunkDirIncrementalResp)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->result
            % obj->newOffset
            % serdes::backedPtr(obj->names, obj->parsed.names)
            % serdes::backedPtr(obj->entryTypes, obj->parsed.entryTypes);
      }

   private:
      int32_t result;
      int64_t newOffset;

      // for serialization
      StringList* names;            // not owned by this object!
      IntList* entryTypes;          // not owned by this object!

      // for deserialization
      struct {
         StringList names;
         IntList entryTypes;
      } parsed;

   public:
      StringList& getNames()
      {
         return *names;
      }

      IntList& getEntryTypes()
      {
         return *entryTypes;
      }

      // getters & setters
      FhgfsOpsErr getResult()
      {
         return (FhgfsOpsErr)this->result;
      }

      int64_t getNewOffset()
      {
         return this->newOffset;
      }

};

#endif /*LISTCHUNKDIRINCREMENTALRESPMSG_H_*/
