#ifndef NETMESSAGE_H_
#define NETMESSAGE_H_

#include <common/net/sock/NetworkInterfaceCard.h>
#include <common/net/sock/Socket.h>
#include <common/toolkit/HighResolutionStats.h>
#include <common/toolkit/serialization/Serialization.h>
#include <common/Common.h>
#include "NetMessageLogHelper.h"
#include "NetMessageTypes.h"


// common message constants
// ========================
#define NETMSG_PREFIX_STR        "fhgfs01" // must be exactly(!!) 8 bytes long
#define NETMSG_PREFIX_STR_LEN    8
#define NETMSG_MIN_LENGTH        NETMSG_HEADER_LENGTH
#define NETMSG_HEADER_LENGTH     24 /* length of the header (see struct NetMessageHeader) */


struct NetMessageHeader
{
   // note: everything in this struct is in host byte order!
   // note: this struct (resp. the header) has a length of 24 bytes (see NETMSG_HEADER_LENGTH)
   
   unsigned       msgLength; // in bytes
   unsigned       msgPaddingInt1; // padding (available for future use)
   char*          msgPrefix; // NETMSG_PREFIX_STR
   unsigned short msgType; // the type of payload, defined as NETMSGTYPE_x
   unsigned short msgPaddingShort1; // padding (available for future use)
   unsigned       msgPaddingInt2; // padding (available for future use)
};

/*
 * This is used as a return code for the testingEquals method defined later
 * We allow undefined here, so we do not neccessarily need to implement the
 * method for all messages. If it is not implemented it will just return
 * UNDEFINED
 */
enum TestingEqualsRes
{
   TestingEqualsRes_FALSE = 0,
   TestingEqualsRes_TRUE = 1,
   TestingEqualsRes_UNDEFINED = 2,
};

class NetMessage
{
   friend class AbstractNetMessageFactory;
   friend class TestMsgSerializationBase;
   
   public:
      virtual ~NetMessage() {}


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
      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
         char* respBuf, size_t bufLen, HighResolutionStats* stats)
      {
         // Note: Has to be implemented appropriately by derived classes.
         // Empty implementation provided here for invalid messages and other messages
         // that don't require this way of processing (e.g. some response messages).

         return false;
      }

      /**
       * Resume processing of a message that requires multiple processing steps.
       * Use needsProcessingResume to control whether this will be called.
       *
       * @throw SocketException on communication error
       */
      virtual bool processIncomingResume(struct sockaddr_in* fromAddr, Socket* sock,
         char* respBuf, size_t bufLen, HighResolutionStats* stats)
      {
         // Note: notes for processIncoming() also apply here.
         // Note: Be aware that the worker thread will change between processinIncoming() and
            // processIncomingResume() so the respBuf will also be a new one

         return false;
      }

      /*
       * Test if a given message is equal to this message. Can be implemented
       * for each message and is used in unit testing framework.
       *
       * @param msg The message to be tested.
       * @return a value according to TestingEqualsRes, which indicates that the
       * message is either equal, not equal or equality is undefined (because
       * method is not implemented)
       */
      virtual TestingEqualsRes testingEquals(NetMessage* msg)
      {
         return TestingEqualsRes_UNDEFINED;
      }

   protected:
      NetMessage(unsigned short msgType)
      {
         this->msgType = msgType;
         this->msgLength = 0;
         this->msgPrefix = NULL;

         this->needsProcessingResume = false;
      }
      

      virtual void serializePayload(char* buf) = 0;
      virtual bool deserializePayload(const char* buf, size_t bufLen) = 0;
      virtual unsigned calcMessageLength() = 0;
      

      // getters & setters
      void setMsgType(unsigned short msgType)
      {
         this->msgType = msgType;
      }
      
      void setNeedsProcessingResume(bool needsProcessingResume)
      {
         this->needsProcessingResume = needsProcessingResume;
      }


   private:
      unsigned       msgLength; // in bytes
      char*          msgPrefix; // NETMSG_PREFIX_STR
      unsigned short msgType; // the type of payload, defined as NETMSGTYPE_x

      bool needsProcessingResume; // true if processings needs to be resumed later via
         // processIncomingResume(), so e.g. socket won't be released etc.


      static void deserializeHeader(char* buf, size_t bufLen, NetMessageHeader* outHeader);
      
      void serializeHeader(char* buf);
      
      
   public:
      // inliners
      
      /**
       * recvBuf must be at least NETMSG_MIN_LENGTH long
       */
      static unsigned extractMsgLengthFromBuf(char* recvBuf)
      {
         unsigned msgLength;
         unsigned msgLengthBufLen;
         Serialization::deserializeUInt(recvBuf, sizeof(msgLength), &msgLength, &msgLengthBufLen);

         return msgLength;
      }

      bool serialize(char* buf, size_t bufLen)
      {
         if(unlikely(bufLen < getMsgLength() ) )
            return false;
         
         serializeHeader(buf);
         serializePayload(&buf[NETMSG_HEADER_LENGTH]);
         
         return true;
      }
      
      void invalidateMsgLength()
      {
         msgLength = 0;
      }
      
      // getters & setters
      unsigned short getMsgType() const
      {
         return msgType;
      }
      
      /**
       * Note: calling this method is expensive, so use it only in error/debug code paths.
       *
       * @return human-readable message type (intended for log messages).
       */
      std::string getMsgTypeStr() const
      {
         return NetMsgStrMapping().defineToStr(msgType);
      }

      unsigned getMsgLength()
      {
         if(!msgLength)
            msgLength = calcMessageLength();
            
         return msgLength;
      }
      
      bool getNeedsProcessingResume() const
      {
         return needsProcessingResume;
      }
};

#endif /*NETMESSAGE_H_*/
