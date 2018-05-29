#ifndef LISTDIRFROMPFFSETRESPMSG_H_
#define LISTDIRFROMPFFSETRESPMSG_H_

#include <common/app/log/LogContext.h>
#include <common/net/message/NetMessage.h>
#include <common/Common.h>

class ListDirFromOffsetRespMsg : public NetMessageSerdes<ListDirFromOffsetRespMsg>
{
   public:
      ListDirFromOffsetRespMsg(FhgfsOpsErr result, StringList* names, UInt8List* entryTypes,
         StringList* entryIDs, Int64List* serverOffsets, int64_t newServerOffset) :
         BaseType(NETMSGTYPE_ListDirFromOffsetResp)
      {
         this->result = result;
         this->names = names;
         this->entryIDs = entryIDs;
         this->entryTypes = entryTypes;
         this->serverOffsets = serverOffsets;
         this->newServerOffset = newServerOffset;
      }

      ListDirFromOffsetRespMsg() : BaseType(NETMSGTYPE_ListDirFromOffsetResp)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->newServerOffset
            % serdes::backedPtr(obj->serverOffsets, obj->parsed.serverOffsets)
            % obj->result
            % serdes::backedPtr(obj->entryTypes, obj->parsed.entryTypes)
            % serdes::backedPtr(obj->entryIDs, obj->parsed.entryIDs)
            % serdes::backedPtr(obj->names, obj->parsed.names);

         serdesCheck(obj, ctx);
      }

   private:
      int32_t result;
      int64_t newServerOffset;

      // for serialization
      StringList* names;    // not owned by this object!
      UInt8List* entryTypes;  // not owned by this object!
      StringList* entryIDs; // not owned by this object!
      Int64List* serverOffsets; // not owned by this object!

      // for deserialization
      struct {
         StringList names;
         UInt8List entryTypes;
         StringList entryIDs;
         Int64List serverOffsets;
      } parsed;

      static void serdesCheck(const ListDirFromOffsetRespMsg*, Serializer&) {}

      static void serdesCheck(ListDirFromOffsetRespMsg* obj, Deserializer& des)
      {
         if(unlikely(
               obj->entryTypes->size() != obj->names->size()
               || obj->entryTypes->size() != obj->entryIDs->size()
               || obj->entryTypes->size() != obj->serverOffsets->size() ) )
         {
            LogContext(__func__).log(Log_WARNING, "Sanity check failed");
            LogContext(__func__).logBacktrace();
            des.setBad();
         }
      }

   public:
      StringList& getNames()
      {
         return *this->names;
      }

      UInt8List& getEntryTypes()
      {
         return *this->entryTypes;
      }

      StringList& getEntryIDs()
      {
         return *this->entryIDs;
      }

      Int64List& getServerOffsets()
      {
         return *this->serverOffsets;
      }

      // getters & setters
      FhgfsOpsErr getResult()
      {
         return (FhgfsOpsErr)this->result;
      }

      int64_t getNewServerOffset()
      {
         return this->newServerOffset;
      }

};

#endif /*LISTDIRFROMPFFSETRESPMSG_H_*/
