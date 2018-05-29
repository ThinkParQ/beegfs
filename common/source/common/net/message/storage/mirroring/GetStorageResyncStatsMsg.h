#ifndef GETSTORAGERESYNCSTATSMSG_H
#define GETSTORAGERESYNCSTATSMSG_H

#include <common/net/message/SimpleUInt16Msg.h>

class GetStorageResyncStatsMsg : public SimpleUInt16Msg
{
   public:
      GetStorageResyncStatsMsg(uint16_t targetID):
         SimpleUInt16Msg(NETMSGTYPE_GetStorageResyncStats, targetID)
      {
      }

      uint16_t getTargetID() const
      {
         return getValue();
      }

   protected:
      GetStorageResyncStatsMsg(): SimpleUInt16Msg(NETMSGTYPE_GetStorageResyncStats)
      {
      }
};

#endif /*GETSTORAGERESYNCSTATSMSG_H*/
