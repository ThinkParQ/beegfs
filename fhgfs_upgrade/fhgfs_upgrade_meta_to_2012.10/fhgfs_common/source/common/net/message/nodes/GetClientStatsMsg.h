#ifndef GETCLIENTSTATSMSG_H_
#define GETCLIENTSTATSMSG_H_

#include <common/net/message/SimpleInt64Msg.h>
#include <common/Common.h>


/**
 * Main object/class to request per-client-stats messages. fhgfs-ctl creates this object and
 * sends it to the server side, which also gets this object then.
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
       * This constructor is called on the receiving side from the net-message-factory, when the
       * the cookieIP is not known yet. The value of cookieIP will be retrieved later, on
       * deserialization of the buffer.
       */
      GetClientStatsMsg() : SimpleInt64Msg(NETMSGTYPE_GetClientStats)
      {
      }
};


#endif /* GETCLIENTSTATSMSG_H_ */
