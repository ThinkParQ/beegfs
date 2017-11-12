#include <common/app/log/LogContext.h>
#include <common/net/message/NetMessageLogHelper.h>
#include "AbstractNetMessageFactory.h"
#include "SimpleMsg.h"


/**
 * @return NetMessage which must be deleted by the caller
 * (msg->msgType is NETMSGTYPE_Invalid on error)
 */
NetMessage* AbstractNetMessageFactory::createFromBuf(char* recvBuf, size_t bufLen)
{
   NetMessageHeader header;
   
   NetMessage::deserializeHeader(recvBuf, bufLen, &header);
   
   NetMessage* msg = createFromMsgType(header.msgType);
      

   char* msgSpecificBuf = recvBuf + NETMSG_HEADER_LENGTH;
   size_t msgSpecificBufLen = bufLen - NETMSG_HEADER_LENGTH;

   bool deserRes = msg->deserializePayload(msgSpecificBuf, msgSpecificBufLen);
   if(unlikely(!deserRes) )
   { // log error with msg type
      NetMsgStrMapping strMapping;

      LogContext("NetMessageFactory buffer deserialization").log(Log_NOTICE,
         "Failed to decode NetMessage: " + strMapping.defineToStr(header.msgType) );

      msg->setMsgType(NETMSGTYPE_Invalid);
   }

   return msg;
}

