#ifndef STORAGERESYNCSTARTEDRESPMSG_H_
#define STORAGERESYNCSTARTEDRESPMSG_H_

#include <common/net/message/SimpleMsg.h>

class StorageResyncStartedRespMsg : public SimpleMsg
{
   public:
      StorageResyncStartedRespMsg() : SimpleMsg(NETMSGTYPE_StorageResyncStartedResp)
      {
      }
};

#endif /*STORAGERESYNCSTARTEDRESPMSG_H_*/
