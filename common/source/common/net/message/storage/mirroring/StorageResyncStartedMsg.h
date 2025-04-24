#pragma once

#include <common/net/message/SimpleUInt16Msg.h>

class StorageResyncStartedMsg : public SimpleUInt16Msg
{
   public:
      StorageResyncStartedMsg(uint16_t buddyTargetID):
         SimpleUInt16Msg(NETMSGTYPE_StorageResyncStarted, buddyTargetID)
      {
      }

      StorageResyncStartedMsg(): SimpleUInt16Msg(NETMSGTYPE_StorageResyncStarted)
      {
      }
};

