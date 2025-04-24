#pragma once

#include <common/Common.h>
#include <common/net/message/SimpleIntMsg.h>

class ChangeTargetConsistencyStatesRespMsg : public SimpleIntMsg
{
   public:
      ChangeTargetConsistencyStatesRespMsg(int result)
         : SimpleIntMsg(NETMSGTYPE_ChangeTargetConsistencyStatesResp, result)
      {
      }

      ChangeTargetConsistencyStatesRespMsg()
         : SimpleIntMsg(NETMSGTYPE_ChangeTargetConsistencyStatesResp)
      {
      }

      FhgfsOpsErr getResult()
      {
         return (FhgfsOpsErr)getValue();
      }
};

