#ifndef REQUESTMETADATAMSGEX_H_
#define REQUESTMETADATAMSGEX_H_

#include <app/App.h>
#include <common/app/log/LogContext.h>
#include <common/net/message/admon/RequestMetaDataMsg.h>
#include <common/net/message/admon/RequestMetaDataRespMsg.h>
#include <nodes/NodeStoreEx.h>

class RequestMetaDataMsgEx : public RequestMetaDataMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /*REQUESTMETADATAMSGEX_H_*/
