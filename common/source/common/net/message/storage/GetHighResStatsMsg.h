#ifndef GETHIGHRESSTATSMSG_H_
#define GETHIGHRESSTATSMSG_H_

#include <common/net/message/SimpleInt64Msg.h>
#include <common/Common.h>


class GetHighResStatsMsg : public SimpleInt64Msg
{
   public:
      /**
       * @param lastStatsTimeMS only the elements with a time value greater than this will be
       * returned in the response
       */
      GetHighResStatsMsg(int64_t lastStatsTimeMS) : SimpleInt64Msg(NETMSGTYPE_GetHighResStats,
         lastStatsTimeMS)
      {
      }

      GetHighResStatsMsg() : SimpleInt64Msg(NETMSGTYPE_GetHighResStats)
      {
      }

};


#endif /* GETHIGHRESSTATSMSG_H_ */
