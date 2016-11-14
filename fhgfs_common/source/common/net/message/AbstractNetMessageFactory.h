#ifndef ABSTRACTNETMESSAGEFACTORY_H_
#define ABSTRACTNETMESSAGEFACTORY_H_

#include <common/net/message/NetMessage.h>


class AbstractNetMessageFactory
{
   public:
      virtual ~AbstractNetMessageFactory() {}
   
      NetMessage* createFromBuf(char* recvBuf, size_t bufLen);
      NetMessage* createFromPreprocessedBuf(NetMessageHeader* header,
         char* msgPayloadBuf, size_t msgPayloadBufLen);
      
      
   protected:
      AbstractNetMessageFactory() {};

      virtual NetMessage* createFromMsgType(unsigned short msgType) = 0;
   
   private:
   
};

#endif /*ABSTRACTNETMESSAGEFACTORY_H_*/
