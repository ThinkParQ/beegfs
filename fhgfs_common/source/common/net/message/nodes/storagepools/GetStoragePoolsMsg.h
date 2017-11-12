#ifndef COMMON_GETSTORAGEPOOLSMSG_H_
#define COMMON_GETSTORAGEPOOLSMSG_H_

#include <common/net/message/SimpleMsg.h>

class GetStoragePoolsMsg : public SimpleMsg
{
   public:
      GetStoragePoolsMsg():
         SimpleMsg(NETMSGTYPE_GetStoragePools) { }
};


#endif /*COMMON_GETSTORAGEPOOLSMSG_H_*/
