#pragma once

#include <common/net/message/SimpleIntMsg.h>

class ModifyStoragePoolRespMsg : public SimpleIntMsg
{
   public:
      /**
       * @param result result of the modify operation
       */
      ModifyStoragePoolRespMsg(FhgfsOpsErr result):
         SimpleIntMsg(NETMSGTYPE_ModifyStoragePoolResp, result) { }

      /**
       * For deserialization only
       */
      ModifyStoragePoolRespMsg() : SimpleIntMsg(NETMSGTYPE_ModifyStoragePoolResp) { }

      FhgfsOpsErr getResult() const
      {
         return static_cast<FhgfsOpsErr>(getValue());
      }
};

