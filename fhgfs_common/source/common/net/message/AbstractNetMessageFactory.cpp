#include <common/app/log/LogContext.h>
#include <common/net/message/NetMessageLogHelper.h>
#include "AbstractNetMessageFactory.h"
#include "SimpleMsg.h"


/**
 * Create NetMessage object (specific type determined by msg header) from a raw msg buffer.
 *
 * @return NetMessage which must be deleted by the caller
 * (msg->msgType is NETMSGTYPE_Invalid on error)
 */
NetMessage* AbstractNetMessageFactory::createFromBuf(char* recvBuf, size_t bufLen)
{
   if(unlikely(bufLen < NETMSG_MIN_LENGTH))
      return new SimpleMsg(NETMSGTYPE_Invalid);

   NetMessageHeader header;

   // decode the message header
   NetMessage::deserializeHeader(recvBuf, bufLen, &header);

   // delegate the rest of the work to another method...

   char* msgPayloadBuf = recvBuf + NETMSG_HEADER_LENGTH;
   size_t msgPayloadBufLen = bufLen - NETMSG_HEADER_LENGTH;

   return createFromPreprocessedBuf(&header, msgPayloadBuf, msgPayloadBufLen);
}

/**
 * Create NetMessage object (specific type determined by msg header) from a raw msg payload buffer,
 * for which the msg header has already been deserialized.
 *
 * @return NetMessage which must be deleted by the caller
 * (msg->msgType is NETMSGTYPE_Invalid on error)
 */
NetMessage* AbstractNetMessageFactory::createFromPreprocessedBuf(NetMessageHeader* header,
   char* msgPayloadBuf, size_t msgPayloadBufLen)
{
   const char* logContext = "NetMsgFactory (create msg from buf)";

   // create the message object for the given message type

   NetMessage* msg = createFromMsgType(header->msgType);
   if(unlikely(msg->getMsgType() == NETMSGTYPE_Invalid) )
   {
      NetMsgStrMapping strMapping;

      LogContext(logContext).log(Log_NOTICE,
         "Received an invalid or unhandled message. "
         "Message type (from raw header): " + strMapping.defineToStr(header->msgType) );

      return msg;
   }

   // apply message feature flags and check compatibility

   msg->setMsgHeaderFeatureFlags(header->msgFeatureFlags);

   bool checkCompatRes = msg->checkHeaderFeatureFlagsCompat();
   if(unlikely(!checkCompatRes) )
   { // incompatible feature flag was set => log error with msg type
      NetMsgStrMapping strMapping;

      LogContext(logContext).log(Log_WARNING,
         "Received a message with incompatible feature flags. "
         "Message type: " + strMapping.defineToStr(header->msgType) + "; "
         "Flags (hex): " + StringTk::uintToHexStr(msg->getMsgHeaderFeatureFlags() ) );

      msg->setMsgType(NETMSGTYPE_Invalid);
      return msg;
   }

   // check whether the header flags are as we expect them:
   //  * if the message does not support mirroring, header flags must be 0
   //  * otherwise, they must not contain flags that are not defined
   if (header->msgFlags & ~(msg->supportsMirroring() ? NetMessageHeader::FlagsMask : 0))
   {
      LOG(WARNING, "Received a message with invalid header flags", header->msgType,
            header->msgFlags);

      msg->setMsgType(NETMSGTYPE_Invalid);
      return msg;
   }

   msg->msgHeader = *header;

   // deserialize message payload

   bool deserRes = msg->deserializePayload(msgPayloadBuf, msgPayloadBufLen);
   if(unlikely(!deserRes) )
   { // deserialization failed => log error with msg type
      NetMsgStrMapping strMapping;

      LogContext(logContext).log(Log_NOTICE,
         "Failed to decode message. "
         "Message type: " + strMapping.defineToStr(header->msgType) );

      msg->setMsgType(NETMSGTYPE_Invalid);
      return msg;
   }

   return msg;
}

