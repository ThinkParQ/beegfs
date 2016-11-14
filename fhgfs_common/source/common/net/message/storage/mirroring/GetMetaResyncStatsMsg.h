#ifndef COMMON_GETMETARESYNCJOBSTATSMSG_H
#define COMMON_GETMETARESYNCJOBSTATSMSG_H

#include <common/net/message/SimpleMsg.h>

class GetMetaResyncStatsMsg : public SimpleMsg
{
   public:
      GetMetaResyncStatsMsg() : SimpleMsg(NETMSGTYPE_GetMetaResyncStats) { }
};

#endif /* COMMON_GETMETARESYNCJOBSTATSMSG_H */
