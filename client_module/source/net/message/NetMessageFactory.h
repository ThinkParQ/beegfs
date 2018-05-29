#ifndef NETMESSAGEFACTORY_H_
#define NETMESSAGEFACTORY_H_

#include <common/Common.h>
#include <common/net/message/NetMessage.h>

extern NetMessage* NetMessageFactory_createFromBuf(App* app, char* recvBuf, size_t bufLen);
extern bool NetMessageFactory_deserializeFromBuf(App* app, char* recvBuf, size_t bufLen,
   NetMessage* outMsg, unsigned short expectedMsgType);
extern NetMessage* NetMessageFactory_createFromMsgType(unsigned short msgType);

#endif /*NETMESSAGEFACTORY_H_*/
