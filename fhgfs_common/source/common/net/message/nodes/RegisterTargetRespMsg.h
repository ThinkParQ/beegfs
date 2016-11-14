#ifndef REGISTERTARGETRESPMSG_H_
#define REGISTERTARGETRESPMSG_H_

#include <common/Common.h>
#include "../SimpleUInt16Msg.h"


class RegisterTargetRespMsg : public SimpleUInt16Msg
{
   public:
      /**
       * @param targetNumID 0 on error (e.g. if given targetNumID from RegisterTargetMsg was
       * rejected), newly assigned numeric ID otherwise (or the old numeric ID value if it was given
       * in RegisterTargetMsg and was accepted).
       */
      RegisterTargetRespMsg(uint16_t targetNumID) :
         SimpleUInt16Msg(NETMSGTYPE_RegisterTargetResp, targetNumID)
      {
      }

      /**
       * For deserialization only
       */
      RegisterTargetRespMsg() : SimpleUInt16Msg(NETMSGTYPE_RegisterTargetResp)
      {
      }


   private:


   public:
      // getters & setters
      uint16_t getTargetNumID()
      {
         return getValue();
      }
};


#endif /* REGISTERTARGETRESPMSG_H_ */
