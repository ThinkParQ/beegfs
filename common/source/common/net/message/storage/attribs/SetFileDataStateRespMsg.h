#pragma once

#include <common/net/message/SimpleIntMsg.h>
#include <common/storage/StorageErrors.h>

class SetFileDataStateRespMsg : public SimpleIntMsg
{
   public:
      SetFileDataStateRespMsg(int result) : SimpleIntMsg(NETMSGTYPE_SetFileDataStateResp, result)
      {
      }

      /**
       * For deserialization only
       */
      SetFileDataStateRespMsg() : SimpleIntMsg(NETMSGTYPE_SetFileDataStateResp)
      {
      }

      FhgfsOpsErr getResult() { return static_cast<FhgfsOpsErr>(getValue()); }
};