#pragma once

#include <common/net/message/SimpleStringMsg.h>

// see class AcknowledgeableMsg (fhgfs_common) for a short description

class AckMsg : public SimpleStringMsg
{
   public:
      AckMsg(const char* ackID) : SimpleStringMsg(NETMSGTYPE_Ack, ackID)
      {
      }

      AckMsg() : SimpleStringMsg(NETMSGTYPE_Ack)
      {
      }
};

