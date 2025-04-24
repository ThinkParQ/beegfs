#pragma once

#include <common/net/message/SimpleStringMsg.h>


class GenericDebugRespMsg : public SimpleStringMsg
{
   public:
      GenericDebugRespMsg(const char* cmdRespStr) :
         SimpleStringMsg(NETMSGTYPE_GenericDebugResp, cmdRespStr)
      {
      }

      GenericDebugRespMsg() : SimpleStringMsg(NETMSGTYPE_GenericDebugResp)
      {
      }


   private:


   public:
      // getters & setters
      const char* getCmdRespStr()
      {
         return getValue();
      }

};


