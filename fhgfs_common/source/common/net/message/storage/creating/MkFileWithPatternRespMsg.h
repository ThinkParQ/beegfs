#ifndef MKFILEWITHPATTERNRESPMSG_H_
#define MKFILEWITHPATTERNRESPMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>

class MkFileWithPatternRespMsg : public NetMessageSerdes<MkFileWithPatternRespMsg>
{
   public:

      /**
       * @param entryInfo just a reference, so do not free it as long as you use this object!
       */
      MkFileWithPatternRespMsg(int result, EntryInfo* entryInfo) :
         BaseType(NETMSGTYPE_MkFileWithPatternResp)
      {
         this->result = result;
         this->entryInfoPtr = entryInfo;
      }

      MkFileWithPatternRespMsg() : BaseType(NETMSGTYPE_MkFileWithPatternResp)
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
         return this->entryInfoPtr;
      }

};

#endif /* MKFILEWITHPATTERNRESPMSG_H_ */
