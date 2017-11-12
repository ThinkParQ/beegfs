#ifndef MKDIRRESPMSG_H_
#define MKDIRRESPMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>

class MkDirRespMsg : public NetMessageSerdes<MkDirRespMsg>
{
   public:

      /**
       * @param ownerNodeID just a reference, so do not free it as long as you use this object!
       * @param entryID just a reference, so do not free it as long as you use this object!
       */
      MkDirRespMsg(int result, EntryInfo* entryInfo) :
         BaseType(NETMSGTYPE_MkDirResp)
      {
         this->result       = result;
         this->entryInfoPtr = entryInfo;
      }

      MkDirRespMsg() : BaseType(NETMSGTYPE_MkDirResp)
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

#endif /*MKDIRRESPMSG_H_*/
