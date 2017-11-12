#ifndef UNMAPTARGETMSG_H_
#define UNMAPTARGETMSG_H_

#include <common/net/message/SimpleUInt16Msg.h>

class UnmapTargetMsg : public SimpleUInt16Msg
{
   public:
      UnmapTargetMsg(uint16_t targetID) : SimpleUInt16Msg(NETMSGTYPE_UnmapTarget, targetID)
      {
      }

      /**
       * Constructor for deserialization only
       */
      UnmapTargetMsg() : SimpleUInt16Msg(NETMSGTYPE_UnmapTarget)
      {
      }


   private:


   public:
      // getters & setters
      uint16_t getTargetID()
      {
         return getValue();
      }
};

#endif /* UNMAPTARGETMSG_H_ */
