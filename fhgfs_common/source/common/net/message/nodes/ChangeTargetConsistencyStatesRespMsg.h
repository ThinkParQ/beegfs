#ifndef CHANGETARGETCONSISTENCYSTATERESPMSG_H_
#define CHANGETARGETCONSISTENCYSTATERESPMSG_H_

#include <common/Common.h>
#include <common/net/message/SimpleIntMsg.h>

class ChangeTargetConsistencyStatesRespMsg : public SimpleIntMsg
{
   public:
      ChangeTargetConsistencyStatesRespMsg(int result)
         : SimpleIntMsg(NETMSGTYPE_ChangeTargetConsistencyStatesResp, result)
      {
      }

      ChangeTargetConsistencyStatesRespMsg()
         : SimpleIntMsg(NETMSGTYPE_ChangeTargetConsistencyStatesResp)
      {
      }

      FhgfsOpsErr getResult()
      {
         return (FhgfsOpsErr)getValue();
      }
};

#endif /*CHANGETARGETCONSISTENCYSTATERESPMSG_H_*/
