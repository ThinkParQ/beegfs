#ifndef GETSTORAGETARGETINFORESPMSG_H
#define GETSTORAGETARGETINFORESPMSG_H

#include <common/net/message/NetMessage.h>
#include <common/storage/StorageTargetInfo.h>

class GetStorageTargetInfoRespMsg: public NetMessageSerdes<GetStorageTargetInfoRespMsg>
{
   public:
      GetStorageTargetInfoRespMsg(StorageTargetInfoList *targetInfoList) :
         BaseType(NETMSGTYPE_GetStorageTargetInfoResp)
      {
         this->targetInfoList = targetInfoList;
      }

      GetStorageTargetInfoRespMsg() : BaseType(NETMSGTYPE_GetStorageTargetInfoResp)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::backedPtr(obj->targetInfoList, obj->parsed.targetInfoList);
      }

   private:
      StorageTargetInfoList* targetInfoList; // not owned by this object!

      // for deserializaztion
      struct {
         StorageTargetInfoList targetInfoList;
      } parsed;

   public:
      const StorageTargetInfoList& getStorageTargetInfos() const
      {
         return *targetInfoList;
      }
};

#endif /*GETSTORAGETARGETINFORESPMSG_H*/
