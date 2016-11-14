#ifndef REGISTERTARGETMSG_H_
#define REGISTERTARGETMSG_H_


#include <common/net/message/NetMessage.h>
#include <common/net/sock/NetworkInterfaceCard.h>
#include <common/Common.h>

/**
 * Registers a new target with the management daemon.
 *
 * This also provides the mechanism to retrieve a numeric target ID from the management node.
 */
class RegisterTargetMsg : public NetMessage
{
   public:

      /**
       * @param targetID just a reference, so do not free it as long as you use this object
       * @param targetNumID set to 0 if no numeric ID was assigned yet and the numeric ID will be
       * set in the reponse
       */
      RegisterTargetMsg(const char* targetID, uint16_t targetNumID) :
            NetMessage(NETMSGTYPE_RegisterTarget)
      {
         this->targetID = targetID;
         this->targetIDLen = strlen(targetID);

         this->targetNumID = targetNumID;
      }


   protected:
      /**
       * For deserialization only
       */
      RegisterTargetMsg() : NetMessage(NETMSGTYPE_RegisterTarget)
      {
      }


      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);

      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenStr(targetIDLen) +
            Serialization::serialLenUShort(); // targetNumID
      }


   private:
      unsigned targetIDLen;
      const char* targetID;
      uint16_t targetNumID; // 0 means "undefined"


   public:

      // getters & setters

      const char* getTargetID()
      {
         return targetID;
      }

      uint16_t getTargetNumID()
      {
         return targetNumID;
      }

};


#endif /* REGISTERTARGETMSG_H_ */
