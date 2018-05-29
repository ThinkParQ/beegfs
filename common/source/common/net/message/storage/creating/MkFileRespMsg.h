#ifndef MKFILERESPMSG_H_
#define MKFILERESPMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>

class MkFileRespMsg : public NetMessageSerdes<MkFileRespMsg>
{
   public:

      /**
       * @param entryInfo just a reference, so do not free it as long as you use this object!
       */
      MkFileRespMsg(int result, EntryInfo* entryInfo) :
         BaseType(NETMSGTYPE_MkFileResp)
      {
         this->result = result;
         this->entryInfoPtr = entryInfo;
      }

      MkFileRespMsg() : BaseType(NETMSGTYPE_MkFileResp)
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

      // for serialization
      EntryInfo* entryInfoPtr;

      // for deserialization
      EntryInfo entryInfo;

   public:

      // inliners

      // getters & setters
      int getResult()
      {
         return result;
      }

      EntryInfo* getEntryInfo(void)
      {
         return &this->entryInfo;
      }

};

#endif /*MKFILERESPMSG_H_*/
