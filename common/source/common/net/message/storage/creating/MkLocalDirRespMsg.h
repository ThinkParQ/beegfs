#ifndef MKLOCALDIRRESPMSG_H_
#define MKLOCALDIRRESPMSG_H_

#include <common/net/message/SimpleIntMsg.h>

class MkLocalDirRespMsg : public SimpleIntMsg
{
   public:
      MkLocalDirRespMsg(FhgfsOpsErr result) : SimpleIntMsg(NETMSGTYPE_MkLocalDirResp, result)
      {
      }

      /**
       * Constructor for deserialization only.
       */
      MkLocalDirRespMsg() : SimpleIntMsg(NETMSGTYPE_MkLocalDirResp)
      {
      }


   private:


   public:
      // getters & setters

      FhgfsOpsErr getResult() const
      {
         return (FhgfsOpsErr)getValue();
      }
};

#endif /*MKLOCALDIRRESPMSG_H_*/
