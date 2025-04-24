#pragma once

#include <common/net/message/SimpleIntMsg.h>
#include <common/storage/StorageErrors.h>

class SetFilePatternRespMsg : public SimpleIntMsg
{
   public:
      SetFilePatternRespMsg(int result) : SimpleIntMsg(NETMSGTYPE_SetFilePatternResp, result)
      {
      }

      /**
       * For deserialization only
       */
      SetFilePatternRespMsg() : SimpleIntMsg(NETMSGTYPE_SetFilePatternResp)
      {
      }

      FhgfsOpsErr getResult() { return static_cast<FhgfsOpsErr>(getValue()); }
};
