#ifndef MAPTARGETSRESPMSG_H_
#define MAPTARGETSRESPMSG_H_

#include <common/Common.h>
#include <common/net/message/SimpleIntMsg.h>


class MapTargetsRespMsg : public SimpleIntMsg
{
   public:
      /**
       * @param result FhgfsOpsErr
       */
      MapTargetsRespMsg(int result) :
         SimpleIntMsg(NETMSGTYPE_MapTargetsResp, result)
      {
      }

      MapTargetsRespMsg() :
         SimpleIntMsg(NETMSGTYPE_MapTargetsResp)
      {
      }
};


#endif /* MAPTARGETSRESPMSG_H_ */
