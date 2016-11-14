#ifndef REQUESTMETADATAMSGEX_H_
#define REQUESTMETADATAMSGEX_H_

#include <common/net/message/admon/RequestMetaDataMsg.h>
#include <common/app/log/LogContext.h>
#include <common/net/message/admon/RequestMetaDataRespMsg.h>
#include <nodes/NodeStoreEx.h>
#include <app/App.h>
#include <common/components/worker/MultiWorkQueue.h>

class RequestMetaDataMsgEx : public RequestMetaDataMsg
{
   public:
      RequestMetaDataMsgEx() : RequestMetaDataMsg()
      {
      }

   private:

   public:

      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock, char* respBuf,
         size_t bufLen, HighResolutionStats *stats);
};

#endif /*REQUESTMETADATAMSGEX_H_*/
