#ifndef FINDOWNERRESPMSG_H_
#define FINDOWNERRESPMSG_H_

#include <common/Common.h>
#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>
#include <common/storage/EntryInfoWithDepth.h>

class FindOwnerRespMsg : public NetMessageSerdes<FindOwnerRespMsg>
{
   public:

      /**
       * @param entryInfo just a reference, so do not free it as long as you use this object!
       */
      FindOwnerRespMsg(int result, EntryInfoWithDepth* entryInfo) :
         BaseType(NETMSGTYPE_FindOwnerResp), result(result), entryInfoPtr(entryInfo)
      {
      }

      FindOwnerRespMsg() : BaseType(NETMSGTYPE_FindOwnerResp)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->result
            % serdes::backedPtr(obj->entryInfoPtr, obj->entryInfo);
      }

   private:
      int32_t result;

      EntryInfoWithDepth* entryInfoPtr; // for serialization
      EntryInfoWithDepth entryInfo; // for deserialization

   public:

      // inliners

      // getters & setters
      int getResult() const
      {
         return result;
      }

      EntryInfoWithDepth& getEntryInfo()
      {
         return entryInfo;
      }
};

#endif /*FINDOWNERRESPMSG_H_*/
