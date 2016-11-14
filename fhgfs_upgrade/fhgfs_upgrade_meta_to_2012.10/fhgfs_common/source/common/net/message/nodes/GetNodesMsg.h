#ifndef GETNODESMSG_H_
#define GETNODESMSG_H_

#include <common/Common.h>
#include "../SimpleIntMsg.h"

class GetNodesMsg : public SimpleIntMsg
{
   public:
      /**
       * @param nodeType NODETYPE_...
       */
      GetNodesMsg(int nodeType) : SimpleIntMsg(NETMSGTYPE_GetNodes, nodeType)
      {
      }
      
      GetNodesMsg() : SimpleIntMsg(NETMSGTYPE_GetNodes)
      {
      }
};


#endif /*GETNODESMSG_H_*/
