#ifndef REMOVENODERESPMSG_H_
#define REMOVENODERESPMSG_H_

#include <common/Common.h>
#include <common/net/message/SimpleIntMsg.h>

class RemoveNodeRespMsg : public SimpleIntMsg
{
   public:
      /**
       * @param result FhgfsOpsErr_... code
       */
      RemoveNodeRespMsg(int result) :
         SimpleIntMsg(NETMSGTYPE_RemoveNodeResp, result)
      {
      }

      RemoveNodeRespMsg() :
         SimpleIntMsg(NETMSGTYPE_RemoveNodeResp)
      {
      }
};


#endif /* REMOVENODERESPMSG_H_ */
