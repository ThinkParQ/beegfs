#pragma once

#include <common/net/message/SimpleIntMsg.h>

class ResyncLocalFileRespMsg : public SimpleIntMsg
{
   public:
      ResyncLocalFileRespMsg(FhgfsOpsErr result) :
         SimpleIntMsg(NETMSGTYPE_ResyncLocalFileResp, result)
      {
      }

      ResyncLocalFileRespMsg() :
         SimpleIntMsg(NETMSGTYPE_ResyncLocalFileResp)
      {
      }

      FhgfsOpsErr getResult()
      {
         return (FhgfsOpsErr)getValue();
      }
};

