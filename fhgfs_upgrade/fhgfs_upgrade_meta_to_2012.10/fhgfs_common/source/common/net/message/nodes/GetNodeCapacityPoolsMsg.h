#ifndef GETNODECAPACITYPOOLSMSG_H_
#define GETNODECAPACITYPOOLSMSG_H_

#include <common/Common.h>
#include "../SimpleIntMsg.h"

class GetNodeCapacityPoolsMsg : public SimpleIntMsg
{
   public:
      /**
       * @param nodeType NODETYPE_...
       */
      GetNodeCapacityPoolsMsg(int nodeType) : SimpleIntMsg(NETMSGTYPE_GetNodeCapacityPools, nodeType)
      {
      }

      GetNodeCapacityPoolsMsg() : SimpleIntMsg(NETMSGTYPE_GetNodeCapacityPools)
      {
      }
};

#endif /* GETNODECAPACITYPOOLSMSG_H_ */
