#ifndef COMMON_REMOVESTORAGEPOOLRESPMSG_H_
#define COMMON_REMOVESTORAGEPOOLRESPMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/StorageErrors.h>

class RemoveStoragePoolRespMsg : public NetMessageSerdes<RemoveStoragePoolRespMsg>
{
   public:

      /**
       * @param result return code of the removal
       */
      RemoveStoragePoolRespMsg(FhgfsOpsErr result):
         BaseType(NETMSGTYPE_RemoveStoragePoolResp), result(result) { }

      /**
       * For deserialization only
       */
      RemoveStoragePoolRespMsg() : BaseType(NETMSGTYPE_RemoveStoragePoolResp){ }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx % obj->result;
      }

      FhgfsOpsErr   getResult() const { return result; };

   private:
      FhgfsOpsErr result;
};

#endif /*COMMON_REMOVESTORAGEPOOLRESPMSG_H_*/
