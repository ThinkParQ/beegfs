#include "NetMessage.h"

/**
 * Processes this message.
 *
 * Note: Some messages might be received over a datagram socket, so the response
 * must be atomic (=> only a single sendto()-call)
 *
 * @param fromAddr must be NULL for stream sockets
 * @return false on error
 */
bool NetMessage_processIncoming(NetMessage* this, struct App* app,
   fhgfs_sockaddr_in* fromAddr, struct Socket* sock, char* respBuf, size_t bufLen)
{
   // Note: Has to be implemented appropriately by derived classes.
   // Empty implementation provided here for invalid messages and other messages
   // that don't require this way of processing (e.g. some response messages).

   return false;
}

/**
 * Returns all feature flags that are supported by this message. Defaults to "none", so this
 * method needs to be overridden by derived messages that actually support header feature
 * flags.
 *
 * @return combination of all supported feature flags
 */
unsigned NetMessage_getSupportedHeaderFeatureFlagsMask(NetMessage* this)
{
   return 0;
}

/**
 * Reads the (common) header part of a message from a buffer.
 *
 * Note: Message type will be set to NETMSGTYPE_Invalid if deserialization fails.
 */
void __NetMessage_deserializeHeader(DeserializeCtx* ctx, NetMessageHeader* outHeader)
{
   size_t totalLength = ctx->length;
   uint64_t prefix = 0;
   // check min buffer length

   if(unlikely(ctx->length < NETMSG_HEADER_LENGTH) )
   {
      outHeader->msgType = NETMSGTYPE_Invalid;
      return;
   }

   // message length
   Serialization_deserializeUInt(ctx, &outHeader->msgLength);

   // verify contained msg length
   if(unlikely(outHeader->msgLength != totalLength) )
   {
      outHeader->msgType = NETMSGTYPE_Invalid;
      return;
   }

   // feature flags
   Serialization_deserializeUShort(ctx, &outHeader->msgFeatureFlags);
   Serialization_deserializeUInt8(ctx, &outHeader->msgCompatFeatureFlags);
   Serialization_deserializeUInt8(ctx, &outHeader->msgFlags);

   // check message prefix
   Serialization_deserializeUInt64(ctx, &prefix);
   if (prefix != NETMSG_PREFIX)
   {
      outHeader->msgType = NETMSGTYPE_Invalid;
      return;
   }

   if (outHeader->msgFlags & ~(MSGHDRFLAG_BUDDYMIRROR_SECOND | MSGHDRFLAG_IS_SELECTIVE_ACK |
                               MSGHDRFLAG_HAS_SEQUENCE_NO))
   {
      outHeader->msgType = NETMSGTYPE_Invalid;
      return;
   }

   // message type
   Serialization_deserializeUShort(ctx, &outHeader->msgType);

   // targetID
   Serialization_deserializeUShort(ctx, &outHeader->msgTargetID);

   // userID
   Serialization_deserializeUInt(ctx, &outHeader->msgUserID);

   Serialization_deserializeUInt64(ctx, &outHeader->msgSequence);
   Serialization_deserializeUInt64(ctx, &outHeader->msgSequenceDone);
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
void __NetMessage_serializeHeader(NetMessage* this, SerializeCtx* ctx, bool zeroLengthField)
{
   // message length
   Serialization_serializeUInt(ctx, zeroLengthField ? 0 : NetMessage_getMsgLength(this) );

   // feature flags
   Serialization_serializeUShort(ctx, this->msgHeader.msgFeatureFlags);
   Serialization_serializeChar(ctx, this->msgHeader.msgCompatFeatureFlags);
   Serialization_serializeChar(ctx, this->msgHeader.msgFlags);

   // message prefix
   Serialization_serializeUInt64(ctx, NETMSG_PREFIX);

   // message type
   Serialization_serializeUShort(ctx, this->msgHeader.msgType);

   // targetID
   Serialization_serializeUShort(ctx, this->msgHeader.msgTargetID);

   // userID
   Serialization_serializeUInt(ctx, this->msgHeader.msgUserID);

   Serialization_serializeUInt64(ctx, this->msgHeader.msgSequence);
   Serialization_serializeUInt64(ctx, this->msgHeader.msgSequenceDone);
}

/**
 * Dummy function for deserialize pointers
 */
bool _NetMessage_deserializeDummy(NetMessage* this, DeserializeCtx* ctx)
{
   printk_fhgfs(KERN_INFO, "Bug: Deserialize function called, although it should not\n");
   dump_stack();

   return true;
}

/**
 * Dummy function for serialize pointers
 */
void _NetMessage_serializeDummy(NetMessage* this, SerializeCtx* ctx)
{
   printk_fhgfs(KERN_INFO, "Bug: Serialize function called, although it should not\n");
   dump_stack();
}
