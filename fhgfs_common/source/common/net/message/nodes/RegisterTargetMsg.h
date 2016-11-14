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
class RegisterTargetMsg : public NetMessageSerdes<RegisterTargetMsg>
{
   public:

      /**
       * @param targetID just a reference, so do not free it as long as you use this object
       * @param targetNumID set to 0 if no numeric ID was assigned yet and the numeric ID will be
       * set in the reponse
       */
      RegisterTargetMsg(const char* targetID, uint16_t targetNumID) :
            BaseType(NETMSGTYPE_RegisterTarget)
      {
         this->targetID = targetID;
         this->targetIDLen = strlen(targetID);

         this->targetNumID = targetNumID;
      }

      /**
       * For deserialization only
       */
      RegisterTargetMsg() : BaseType(NETMSGTYPE_RegisterTarget)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::rawString(obj->targetID, obj->targetIDLen)
            % obj->targetNumID;
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
