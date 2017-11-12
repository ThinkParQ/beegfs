#ifndef REQUESTMETADATAMSG_H_
#define REQUESTMETADATAMSG_H_

#include <common/net/message/SimpleInt64Msg.h>
#include <common/net/message/NetMessageTypes.h>

class RequestMetaDataMsg : public SimpleInt64Msg
{
public:
   /**
    * @param lastStatsTimeMS only for high res stats
    * only the elements with a time value greater than this will be
    * returned in the response
    */
   RequestMetaDataMsg(int64_t lastStatsTimeMS) : SimpleInt64Msg
       (NETMSGTYPE_RequestMetaData, lastStatsTimeMS)
   {
   }

   RequestMetaDataMsg() : SimpleInt64Msg(NETMSGTYPE_RequestMetaData,0)
   {
   }
};

#endif /*REQUESTMETADATAMSG_H_*/
