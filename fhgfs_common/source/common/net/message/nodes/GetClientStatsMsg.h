#ifndef GETCLIENTSTATSMSG_H_
#define GETCLIENTSTATSMSG_H_

#include <common/net/message/SimpleInt64Msg.h>
#include <common/Common.h>

#define GETCLIENTSTATSMSG_FLAG_PERUSERSTATS      1 /* query per-user (instead per-client) stats */


/**
 * Request per-client or per-user operation statictics.
 */
class GetClientStatsMsg : public SimpleInt64Msg
{
   public:
      /**
       * @param cookieIP  - Not all clients fit into the last stats message. So we use this IP
       *                    as a cookie to know where to continue (IP + 1)
       *
       *  This constructor is called on the side sending the mesage.
       */
      GetClientStatsMsg(int64_t cookieIP) : SimpleInt64Msg(NETMSGTYPE_GetClientStats, cookieIP)
      {
      }

      /**
       * For dersialization only.
       */
      GetClientStatsMsg() : SimpleInt64Msg(NETMSGTYPE_GetClientStats)
      {
      }


   protected:
      unsigned getSupportedHeaderFeatureFlagsMask() const
      {
         return GETCLIENTSTATSMSG_FLAG_PERUSERSTATS;
      }


   public:
      uint64_t getCookieIP()
      {
         return getValue();
      }
};


#endif /* GETCLIENTSTATSMSG_H_ */
