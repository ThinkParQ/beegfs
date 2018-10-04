#ifndef ABSTRACTNETMESSAGEFACTORY_H_
#define ABSTRACTNETMESSAGEFACTORY_H_

#include <common/net/message/NetMessage.h>


class AbstractNetMessageFactory
{
   public:
      virtual ~AbstractNetMessageFactory() {}

      std::unique_ptr<NetMessage> createFromRaw(char* recvBuf, size_t bufLen) const;
      std::unique_ptr<NetMessage> createFromPreprocessedBuf(NetMessageHeader* header,
         char* msgPayloadBuf, size_t msgPayloadBufLen) const;

      std::unique_ptr<NetMessage> createFromBuf(std::vector<char> buf) const
      {
         auto result = createFromRaw(&buf[0], buf.size());
         if (result)
            result->backingBuffer = std::move(buf);
         return result;
      }

   protected:
      AbstractNetMessageFactory() {};

      virtual std::unique_ptr<NetMessage> createFromMsgType(unsigned short msgType) const = 0;
};

#endif /*ABSTRACTNETMESSAGEFACTORY_H_*/
