#pragma once

#include <common/net/message/SimpleIntMsg.h>

class RmLocalDirRespMsg : public SimpleIntMsg
{
   public:
      RmLocalDirRespMsg(FhgfsOpsErr result) : SimpleIntMsg(NETMSGTYPE_RmLocalDirResp, result)
      {
      }

      /**
       * Constructor for deserialization only.
       */
      RmLocalDirRespMsg() : SimpleIntMsg(NETMSGTYPE_RmLocalDirResp)
      {
      }


   private:


   public:
      // getters & setters

      FhgfsOpsErr getResult() const
      {
         return (FhgfsOpsErr)getValue();
      }
};

