#ifndef REQUESTSTORAGEDATAMSG_H_
#define REQUESTSTORAGEDATAMSG_H_

#include <common/net/message/SimpleInt64Msg.h>
#include <common/net/message/NetMessageTypes.h>

class RequestStorageDataMsg : public SimpleInt64Msg
{
public:
   /**
    * @param lastStatsTimeMS only for high res stats
    * only the elements with a time value greater than this will be
    * returned in the response
    */
   RequestStorageDataMsg(int64_t lastStatsTimeMS) : SimpleInt64Msg(NETMSGTYPE_RequestStorageData,
      lastStatsTimeMS)
      {
      }

	RequestStorageDataMsg()  : SimpleInt64Msg(NETMSGTYPE_RequestStorageData,0)
   {
   };
};

#endif /*REQUESTSTORAGEDATAMSG_H_*/
