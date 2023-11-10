#ifndef NETMESSAGE_H_
#define NETMESSAGE_H_

#include <common/net/sock/NetworkInterfaceCard.h>
#include <common/net/sock/Socket.h>
#include <common/toolkit/HighResolutionStats.h>
#include <common/toolkit/serialization/Serialization.h>
#include <common/Common.h>
#include "NetMessageLogHelper.h"
#include "NetMessageTypes.h"

#include <climits>


// common message constants
// ========================
#define NETMSG_MIN_LENGTH        NETMSG_HEADER_LENGTH
#define NETMSG_HEADER_LENGTH     40 /* length of the header (see struct NetMessageHeader) */
#define NETMSG_MAX_MSG_SIZE      65536    // 64kB
#define NETMSG_MAX_PAYLOAD_SIZE  ((unsigned)(NETMSG_MAX_MSG_SIZE - NETMSG_HEADER_LENGTH))

#define NETMSG_DEFAULT_USERID    (~0) // non-zero to avoid mixing up with root userID

struct NetMessageHeader
{
   static constexpr const uint64_t MSG_PREFIX = (0x42474653ULL << 32) + BEEGFS_DATA_VERSION;

   static const uint8_t Flag_BuddyMirrorSecond = 0x01;
   static const uint8_t Flag_IsSelectiveAck    = 0x02;
   static const uint8_t Flag_HasSequenceNumber = 0x04;

   static const uint8_t FlagsMask = 0x07;

   uint32_t       msgLength; // in bytes
   uint16_t       msgFeatureFlags; // feature flags for derived messages (depend on msgType)
   uint8_t        msgCompatFeatureFlags;
   uint8_t        msgFlags;
   uint16_t       msgType; // the type of payload, defined as NETMSGTYPE_x
   uint16_t       msgTargetID; // targetID (not groupID) for per-target workers on storage server
   uint32_t       msgUserID; // system user ID for per-user msg queues, stats etc.
   uint64_t       msgSequence; // for retries, 0 if not present
   uint64_t       msgSequenceDone; // a sequence number that has been fully processed, or 0

   static void fixLengthField(Serializer& ser, uint32_t actualLength)
   {
      ser % actualLength;
   }

   template<typename This, typename Ctx>
   static void serialize(This obj, Ctx& ctx)
   {
      uint64_t prefix = MSG_PREFIX;

      ctx
         % obj->msgLength
         % obj->msgFeatureFlags
         % obj->msgCompatFeatureFlags
         % obj->msgFlags
         % prefix
         % obj->msgType
         % obj->msgTargetID
         % obj->msgUserID
         % obj->msgSequence
         % obj->msgSequenceDone;

      checkPrefix(ctx, prefix);
   }

   static void checkPrefix(Deserializer& des, uint64_t prefix)
   {
      if (unlikely(!des.good() ) || prefix != MSG_PREFIX)
         des.setBad();
   }

   static void checkPrefix(Serializer& ser, uint64_t prefix)
   {
   }

   static unsigned extractMsgLengthFromBuf(const char* recvBuf, unsigned bufLen)
   {
      Deserializer des(recvBuf, bufLen);
      uint32_t length;
      des % length;
      // return -1 if the buffer was short, which will also be larger than any possible message
      // we can possibly receive (due to 16bit message length)
      return des.good() ? length : -1;
   }
};

class NetMessage
{
   friend class AbstractNetMessageFactory;
   friend class TestMsgSerializationBase;

   public:
      virtual ~NetMessage() {}

      static void deserializeHeader(char* buf, size_t bufLen, NetMessageHeader* outHeader)
      {
         Deserializer des(buf, bufLen);
         des % *outHeader;
         if(unlikely(!des.good()))
            outHeader->msgType = NETMSGTYPE_Invalid;
      }

      class ResponseContext
      {
         friend class NetMessage;

         public:
            ResponseContext(struct sockaddr_in* fromAddr, Socket* sock, char* respBuf,
               unsigned bufLen, HighResolutionStats* stats, bool locallyGenerated = false)
               : fromAddr(fromAddr), socket(sock), responseBuffer(respBuf),
                 responseBufferLength(bufLen), stats(stats), locallyGenerated(locallyGenerated)
            {}

            void sendResponse(const NetMessage& response) const
            {
               unsigned msgLength =
                  response.serializeMessage(responseBuffer, responseBufferLength).second;
               socket->sendto(responseBuffer, msgLength, 0,
                  reinterpret_cast<const sockaddr*>(fromAddr), sizeof(*fromAddr) );
            }

            HighResolutionStats* getStats() const { return stats; }
            Socket* getSocket() const { return socket; }
            char* getBuffer() const { return responseBuffer; }
            unsigned getBufferLength() const { return responseBufferLength; }

            std::string peerName() const
            {
               return fromAddr ? Socket::ipaddrToStr(fromAddr->sin_addr) : socket->getPeername();
            }

            bool isLocallyGenerated() const { return locallyGenerated; }

         private:
            struct sockaddr_in* fromAddr;
            Socket* socket;
            char* responseBuffer;
            unsigned responseBufferLength;
            HighResolutionStats* stats;
            bool locallyGenerated;
      };

      /**
       * Processes this incoming message.
       *
       * Note: Some messages might be received over a datagram socket, so the response
       * must be atomic (=> only a single sendto()-call)
       *
       * @param fromAddr must be NULL for stream sockets
       * @return false if the client should be disconnected (e.g. because it sent invalid data)
       * @throw SocketException on communication error
       */
      virtual bool processIncoming(ResponseContext& ctx)
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
      virtual unsigned getSupportedHeaderFeatureFlagsMask() const
      {
         return 0;
      }

      virtual bool supportsMirroring() const { return false; }

   protected:
      NetMessage(unsigned short msgType)
      {
         this->msgHeader.msgLength = 0;
         this->msgHeader.msgFeatureFlags = 0;
         this->msgHeader.msgCompatFeatureFlags = 0;
         this->msgHeader.msgFlags = 0;
         this->msgHeader.msgType = msgType;
         this->msgHeader.msgUserID = NETMSG_DEFAULT_USERID;
         this->msgHeader.msgTargetID = 0;
         this->msgHeader.msgSequence = 0;
         this->msgHeader.msgSequenceDone = 0;

         this->releaseSockAfterProcessing = true;
      }


      virtual void serializePayload(Serializer& ser) const = 0;
      virtual bool deserializePayload(const char* buf, size_t bufLen) = 0;

   private:
      NetMessageHeader msgHeader;

      bool releaseSockAfterProcessing; /* false if sock was already released during
                                          processIncoming(), e.g. due to early response. */

      std::vector<char> backingBuffer;

   public:
      std::pair<bool, unsigned> serializeMessage(char* buf, size_t bufLen) const
      {
         Serializer ser(buf, bufLen);
         Serializer atStart = ser.mark();

         ser % msgHeader;
         serializePayload(ser);

         // fix message length in header and serialize header again to fix the message length
         NetMessageHeader::fixLengthField(atStart, ser.size() );

         return std::make_pair(ser.good(), ser.size() );
      }

      /**
       * Check if the msg sender has set an incompatible feature flag.
       *
       * @return false if an incompatible feature flag was set
       */
      bool checkHeaderFeatureFlagsCompat() const
      {
         unsigned unsupportedFlags = ~getSupportedHeaderFeatureFlagsMask();
         if(unlikely(msgHeader.msgFeatureFlags & unsupportedFlags) )
            return false; // an unsupported flag was set

         return true;
      }

      // getters & setters

      unsigned short getMsgType() const
      {
         return msgHeader.msgType;
      }

      /**
       * Note: calling this method is expensive, so use it only in error/debug code paths.
       *
       * @return human-readable message type (intended for log messages).
       */
      std::string getMsgTypeStr() const
      {
         return netMessageTypeToStr(msgHeader.msgType);
      }

      unsigned getMsgHeaderFeatureFlags() const
      {
         return msgHeader.msgFeatureFlags;
      }

      /**
       * Note: You probably rather want to call addMsgHeaderFeatureFlag() instead of this.
       */
      void setMsgHeaderFeatureFlags(unsigned msgFeatureFlags)
      {
         this->msgHeader.msgFeatureFlags = msgFeatureFlags;
      }

      /**
       * Test flag. (For convenience and readability.)
       *
       * @return true if given flag is set.
       */
      bool isMsgHeaderFeatureFlagSet(unsigned flag) const
      {
         return (this->msgHeader.msgFeatureFlags & flag) != 0;
      }

      /**
       * Add another flag without clearing the previously set flags.
       *
       * Note: The receiver will reject this message if it doesn't know the given feature flag.
       */
      void addMsgHeaderFeatureFlag(unsigned flag)
      {
         this->msgHeader.msgFeatureFlags |= flag;
      }

      /**
       * Remove a certain flag without clearing the other previously set flags.
       */
      void unsetMsgHeaderFeatureFlag(unsigned flag)
      {
         this->msgHeader.msgFeatureFlags &= ~flag;
      }

      uint8_t getMsgHeaderCompatFeatureFlags() const
      {
         return msgHeader.msgCompatFeatureFlags;
      }

      void setMsgHeaderCompatFeatureFlags(uint8_t msgCompatFeatureFlags)
      {
         this->msgHeader.msgCompatFeatureFlags = msgCompatFeatureFlags;
      }

      /**
       * Test flag. (For convenience and readability.)
       *
       * @return true if given flag is set.
       */
      bool isMsgHeaderCompatFeatureFlagSet(uint8_t flag) const
      {
         return (this->msgHeader.msgCompatFeatureFlags & flag) != 0;
      }

      /**
       * Add another flag without clearing the previously set flags.
       *
       * Note: "compat" means these flags might not be understood and will then just be ignored by
       * the receiver (e.g. if the receiver is an older fhgfs version).
       */
      void addMsgHeaderCompatFeatureFlag(uint8_t flag)
      {
         this->msgHeader.msgCompatFeatureFlags |= flag;
      }

      unsigned getMsgHeaderUserID() const
      {
         return msgHeader.msgUserID;
      }

      void setMsgHeaderUserID(unsigned userID)
      {
         this->msgHeader.msgUserID = userID;
      }

      unsigned getMsgHeaderTargetID() const
      {
         return msgHeader.msgTargetID;
      }

      /**
       * @param targetID this has to be an actual targetID (not a groupID); (default is 0 if
       * targetID is not applicable).
       */
      void setMsgHeaderTargetID(uint16_t targetID)
      {
         this->msgHeader.msgTargetID = targetID;
      }

      bool getReleaseSockAfterProcessing() const
      {
         return releaseSockAfterProcessing;
      }

      void setReleaseSockAfterProcessing(bool releaseSockAfterProcessing)
      {
         this->releaseSockAfterProcessing = releaseSockAfterProcessing;
      }

      uint64_t getSequenceNumber() const { return msgHeader.msgSequence; }
      void     setSequenceNumber(uint64_t value) { msgHeader.msgSequence = value; }

      uint64_t getSequenceNumberDone() const { return msgHeader.msgSequenceDone; }
      void     setSequenceNumberDone(uint64_t value) { msgHeader.msgSequenceDone = value; }

      uint32_t getLength() const { return msgHeader.msgLength; }

      uint8_t getFlags() const { return msgHeader.msgFlags; }
      bool hasFlag(uint8_t flag) const { return msgHeader.msgFlags & flag; }
      void addFlag(uint8_t flag) { msgHeader.msgFlags |= flag; }
      void removeFlag(uint8_t flag) { msgHeader.msgFlags &= ~flag; }
};

template<typename Derived>
class NetMessageSerdes : public NetMessage
{
   protected:
      typedef NetMessageSerdes<Derived> BaseType;

      NetMessageSerdes(unsigned short msgType)
         : NetMessage(msgType)
      {}

      void serializePayload(Serializer& ser) const
      {
         ser % *(Derived*) this;
      }

      bool deserializePayload(const char* buf, size_t bufLen)
      {
         Deserializer des(buf, bufLen);
         des % *(Derived*) this;
         return des.good();
      }
};


template<typename Derived>
class MirroredMessageBase : public NetMessage
{
   public:
      std::pair<NodeType, NumNodeID> getRequestorID(NetMessage::ResponseContext& ctx) const
      {
         if (this->hasFlag(NetMessageHeader::Flag_BuddyMirrorSecond))
            return {requestorNodeType, requestorNodeID};
         else
            return {ctx.getSocket()->getNodeType(), ctx.getSocket()->getNodeID()};
      }

      void setRequestorID(std::pair<NodeType, NumNodeID> id)
      {
         std::tie(requestorNodeType, requestorNodeID) = id;
      }

   protected:
      typedef MirroredMessageBase<Derived> BaseType;

      MirroredMessageBase(unsigned short msgType)
         : NetMessage(msgType)
      {}

      void serializePayload(Serializer& ser) const
      {
         ser % *(Derived*) this;

         if (hasFlag(NetMessageHeader::Flag_BuddyMirrorSecond))
            ser
               % serdes::as<uint8_t>(requestorNodeType)
               % requestorNodeID;
      }

      bool deserializePayload(const char* buf, size_t bufLen)
      {
         Deserializer des(buf, bufLen);
         des % *(Derived*) this;

         if (hasFlag(NetMessageHeader::Flag_BuddyMirrorSecond))
            des
               % serdes::as<uint8_t>(requestorNodeType)
               % requestorNodeID;

         return des.good();
      }

   private:
      NumNodeID requestorNodeID;
      NodeType requestorNodeType;
};

#endif /*NETMESSAGE_H_*/
