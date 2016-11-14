#ifndef ACKMSGEX_H_
#define ACKMSGEX_H_

#include "../SimpleStringMsg.h"


struct AckMsgEx;
typedef struct AckMsgEx AckMsgEx;

static inline void AckMsgEx_init(AckMsgEx* this);
static inline void AckMsgEx_initFromValue(AckMsgEx* this,
   const char* value);

// virtual functions
extern bool __AckMsgEx_processIncoming(NetMessage* this, struct App* app,
   fhgfs_sockaddr_in* fromAddr, struct Socket* sock, char* respBuf, size_t bufLen);

// getters & setters
static inline const char* AckMsgEx_getValue(AckMsgEx* this);


struct AckMsgEx
{
   SimpleStringMsg simpleStringMsg;
};


void AckMsgEx_init(AckMsgEx* this)
{
   SimpleStringMsg_init( (SimpleStringMsg*)this, NETMSGTYPE_Ack);
}

void AckMsgEx_initFromValue(AckMsgEx* this, const char* value)
{
   SimpleStringMsg_initFromValue( (SimpleStringMsg*)this, NETMSGTYPE_Ack, value);
}

const char* AckMsgEx_getValue(AckMsgEx* this)
{
   return SimpleStringMsg_getValue( (SimpleStringMsg*)this);
}


#endif /* ACKMSGEX_H_ */
