#ifndef REGISTERNODERESPMSG_H_
#define REGISTERNODERESPMSG_H_

#include <common/Common.h>
#include "../SimpleUInt16Msg.h"


class RegisterNodeRespMsg : public SimpleUInt16Msg
{
   public:
      /**
       * @param nodeNumID 0 on error (e.g. if given nodeNumID from RegisterNodeMsg was rejected),
       * newly assigned numeric ID otherwise (or the old numeric ID value if it was given in
       * RegisterNodeMsg and was accepted).
       */
      RegisterNodeRespMsg(uint16_t nodeNumID) :
         SimpleUInt16Msg(NETMSGTYPE_RegisterNodeResp, nodeNumID)
      {
      }

      /**
       * For deserialization only
       */
      RegisterNodeRespMsg() : SimpleUInt16Msg(NETMSGTYPE_RegisterNodeResp)
      {
      }


   private:


   public:
      // getters & setters
      uint16_t getNodeNumID()
      {
         return getValue();
      }
};


#endif /* REGISTERNODERESPMSG_H_ */
