#include "NetMessage.h"
#include <common/toolkit/serialization/Serialization.h>

/**
 * Reads the (common) header part of a message from a buffer.
 */
void NetMessage::deserializeHeader(char* buf, size_t bufLen, NetMessageHeader* outHeader)
{
   size_t bufPos = 0;
   
   if(unlikely(bufLen < NETMSG_HEADER_LENGTH) )
   {
      outHeader->msgType = NETMSGTYPE_Invalid;
      return;
   }

   // message length
   
   unsigned msgLenBufLen = 0;
   Serialization::deserializeUInt(&buf[bufPos], bufLen-bufPos, &outHeader->msgLength,
      &msgLenBufLen);
   bufPos += msgLenBufLen;
   
   if(unlikely(outHeader->msgLength != bufLen) )
   {
      outHeader->msgType = NETMSGTYPE_Invalid;
      return;
   }
   
   bufPos += sizeof(unsigned); // position after paddingInt1

   // message prefix
   
   if(*(int64_t*) NETMSG_PREFIX_STR != *(int64_t*)&buf[bufPos] )
   {
      outHeader->msgType = NETMSGTYPE_Invalid;
      return;
   }

   bufPos += NETMSG_PREFIX_STR_LEN; // position after msgPrefix
   
   // message type
   
   unsigned msgTypeBufLen;
   Serialization::deserializeUShort(&buf[bufPos], bufLen-bufPos, &outHeader->msgType,
      &msgTypeBufLen);
   bufPos += msgTypeBufLen;
   
   
   bufPos += sizeof(unsigned short); // position after paddingShort1
   bufPos += sizeof(unsigned); // position after paddingInt2
}


/**
 * Writes the (common) header part of a message to a buffer.
 * 
 * Note the min required size for the buf parameter! Message-specific data can be stored
 * from &buf[NETMSG_HEADER_LENGTH] on.
 * The msg->msgPrefix field is ignored and will always be stored correctly in the buffer.
 * 
 * @param buf min size is NETMSG_HEADER_LENGTH
 * @return false on error (e.g. bufLen too small), true otherwise
 */
void NetMessage::serializeHeader(char* buf)
{
   size_t bufPos = 0;
   
   // message length
   
   bufPos += Serialization::serializeUInt(&buf[bufPos], getMsgLength() );

   bufPos += sizeof(unsigned); // position after paddingInt1
   
   // message prefix
   
   memcpy(&buf[bufPos], NETMSG_PREFIX_STR, NETMSG_PREFIX_STR_LEN);
   
   bufPos += NETMSG_PREFIX_STR_LEN; // position after msgPrefix

   // message type
   
   bufPos += Serialization::serializeUShort(&buf[bufPos], this->msgType);

   
   bufPos += sizeof(unsigned short); // position after paddingShort1
   bufPos += sizeof(unsigned); // position after paddingInt2
}


