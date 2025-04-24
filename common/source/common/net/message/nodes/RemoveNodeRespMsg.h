#pragma once

#include <common/Common.h>
#include <common/net/message/SimpleIntMsg.h>

class RemoveNodeRespMsg : public SimpleIntMsg
{
   public:
      /**
       * @param result FhgfsOpsErr_... code
       */
      RemoveNodeRespMsg(int result) :
         SimpleIntMsg(NETMSGTYPE_RemoveNodeResp, result)
      {
      }

      RemoveNodeRespMsg() :
         SimpleIntMsg(NETMSGTYPE_RemoveNodeResp)
      {
      }
};


