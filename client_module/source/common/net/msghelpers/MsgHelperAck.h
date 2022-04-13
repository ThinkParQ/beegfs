#ifndef MSGHELPERACK_H_
#define MSGHELPERACK_H_

#include <common/net/message/control/AckMsgEx.h>
#include <common/Common.h>
#include <components/DatagramListener.h>
#include <app/App.h>


// inliners
static inline bool MsgHelperAck_respondToAckRequest(App* app, const char* ackID,
   fhgfs_sockaddr_in* fromAddr, Socket* sock, char* respBuf, size_t bufLen);


/**
 * Note: This will only send a response if an ackID has been set.
 *
 * @return true if ackID was set
 */
bool MsgHelperAck_respondToAckRequest(App* app, const char* ackID,
   fhgfs_sockaddr_in* fromAddr, Socket* sock, char* respBuf, size_t bufLen)
{
   Logger* log = App_getLogger(app);
   const char* logContext = "Ack response";

   AckMsgEx respMsg;
   unsigned respLen;
   bool serializeRes;
   ssize_t sendRes;

   if(!StringTk_hasLength(ackID) )
      return false;

   AckMsgEx_initFromValue(&respMsg, ackID);

   respLen = NetMessage_getMsgLength( (NetMessage*)&respMsg);
   serializeRes = NetMessage_serialize( (NetMessage*)&respMsg, respBuf, bufLen);
   if(unlikely(!serializeRes) )
   {
      Logger_logErrFormatted(log, logContext, "Unable to serialize response");
      goto err_uninit;
   }

   if(fromAddr)
   { // datagram => sync via dgramLis send method
      DatagramListener* dgramLis = App_getDatagramListener(app);
      sendRes = DatagramListener_sendto_kernel(dgramLis, respBuf, respLen, 0, fromAddr);
   }
   else
      sendRes = Socket_sendto_kernel(sock, respBuf, respLen, 0, NULL);

   if(unlikely(sendRes <= 0) )
      Logger_logErrFormatted(log, logContext, "Send error. ErrCode: %lld", (long long)sendRes);

err_uninit:
   return true;
}

#endif /* MSGHELPERACK_H_ */
